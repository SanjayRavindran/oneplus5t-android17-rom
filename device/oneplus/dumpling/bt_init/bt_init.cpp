#define LOG_TAG "oneplus5t-bt-init"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <log/log.h>

#define BTPROTO_HCI 1
#define HCI_CHANNEL_CONTROL 3
#define HCI_DEV_NONE 0xffff

#define MGMT_OP_READ_INDEX_LIST 0x0003
#define MGMT_OP_READ_UNCONF_INDEX_LIST 0x0036
#define MGMT_OP_SET_PUBLIC_ADDRESS 0x0039
#define MGMT_EV_CMD_COMPLETE 0x0001
#define MGMT_EV_CMD_STATUS 0x0002
#define MGMT_INDEX_NONE 0xffff

struct sockaddr_hci {
    sa_family_t hci_family;
    unsigned short hci_dev;
    unsigned short hci_channel;
};

struct __attribute__((packed)) mgmt_hdr {
    uint16_t opcode;
    uint16_t index;
    uint16_t len;
};

struct __attribute__((packed)) mgmt_pkt {
    uint16_t opcode;
    uint16_t index;
    uint16_t len;
    uint8_t data[1024];
};

static bool load_bdaddr(uint8_t bdaddr[6]) {
    // Stock QCOM persist BT NV file format: 01 01 06 <LSB..MSB 6 bytes>
    int fd = open("/mnt/vendor/persist/bluetooth/.bt_nv.bin", O_RDONLY);
    if (fd >= 0) {
        uint8_t buf[16] = {};
        ssize_t n = read(fd, buf, sizeof(buf));
        close(fd);
        if (n >= 9 && buf[0] == 0x01 && buf[2] == 0x06) {
            bool all_zero = true;
            for (int i = 0; i < 6; i++) {
                bdaddr[i] = buf[3 + i];
                if (bdaddr[i] != 0) all_zero = false;
            }
            if (!all_zero) {
                ALOGI("Loaded factory BD_ADDR %02x:%02x:%02x:%02x:%02x:%02x from .bt_nv.bin",
                      bdaddr[5], bdaddr[4], bdaddr[3], bdaddr[2], bdaddr[1], bdaddr[0]);
                return true;
            }
        }
    }

    // Fallback: deterministic locally-administered unicast address from serialno
    uint64_t hash = 0x51a050108998ULL;
    FILE* f = fopen("/proc/cmdline", "r");
    if (f) {
        char line[2048] = {};
        if (fgets(line, sizeof(line), f)) {
            const char* s = strstr(line, "androidboot.serialno=");
            if (s) {
                s += strlen("androidboot.serialno=");
                while (*s && *s != ' ' && *s != '\n') {
                    hash = hash * 1315423911ULL + static_cast<uint8_t>(*s++);
                }
            }
        }
        fclose(f);
    }
    for (int i = 0; i < 6; i++) {
        bdaddr[i] = static_cast<uint8_t>((hash >> (i * 8)) & 0xff);
    }
    bdaddr[5] = (bdaddr[5] & 0xfe) | 0x02;
    ALOGW("Using derived fallback BD_ADDR %02x:%02x:%02x:%02x:%02x:%02x",
          bdaddr[5], bdaddr[4], bdaddr[3], bdaddr[2], bdaddr[1], bdaddr[0]);
    return true;
}

static bool send_mgmt_cmd(int fd, uint16_t opcode, uint16_t index,
                          const void* data, uint16_t len,
                          uint8_t* out_status, uint8_t* out_buf, uint16_t* out_len) {
    struct mgmt_pkt cmd = {};
    cmd.opcode = opcode;
    cmd.index = index;
    cmd.len = len;
    if (len > 0 && data) {
        memcpy(cmd.data, data, len);
    }

    ssize_t req_sz = 6 + len;
    if (write(fd, &cmd, req_sz) != req_sz) {
        return false;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
    for (int tries = 0; tries < 20; tries++) {
        int r = poll(&pfd, 1, 200);
        if (r <= 0) return false;

        struct mgmt_pkt ev = {};
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n < 6) continue;

        if (ev.opcode == MGMT_EV_CMD_COMPLETE && ev.len >= 3) {
            uint16_t cmd_op = ev.data[0] | (static_cast<uint16_t>(ev.data[1]) << 8);
            if (cmd_op == opcode && ev.index == index) {
                *out_status = ev.data[2];
                uint16_t plen = ev.len - 3;
                if (out_buf && out_len) {
                    uint16_t copy_len = plen < *out_len ? plen : *out_len;
                    memcpy(out_buf, ev.data + 3, copy_len);
                    *out_len = plen;
                }
                return true;
            }
        } else if (ev.opcode == MGMT_EV_CMD_STATUS && ev.len >= 3) {
            uint16_t cmd_op = ev.data[0] | (static_cast<uint16_t>(ev.data[1]) << 8);
            if (cmd_op == opcode && ev.index == index) {
                *out_status = ev.data[2];
                if (out_len) *out_len = 0;
                return true;
            }
        }
    }
    return false;
}

static bool index_in_list(int fd, uint16_t list_op, uint16_t target) {
    uint8_t status = 0xff;
    uint8_t buf[256] = {};
    uint16_t len = sizeof(buf);
    if (!send_mgmt_cmd(fd, list_op, MGMT_INDEX_NONE, nullptr, 0, &status, buf, &len)) {
        return false;
    }
    if (status != 0 || len < 2) return false;
    uint16_t count = buf[0] | (static_cast<uint16_t>(buf[1]) << 8);
    for (uint16_t i = 0; i < count && (2 + (i + 1) * 2) <= len; i++) {
        uint16_t idx = buf[2 + i * 2] | (static_cast<uint16_t>(buf[3 + i * 2]) << 8);
        if (idx == target) return true;
    }
    return false;
}

int main() {
    uint8_t bdaddr[6] = {};
    load_bdaddr(bdaddr);

    int fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (fd < 0) {
        ALOGE("socket(PF_BLUETOOTH) failed: %s", strerror(errno));
        return 1;
    }

    struct sockaddr_hci addr = {};
    addr.hci_family = AF_BLUETOOTH;
    addr.hci_dev = HCI_DEV_NONE;
    addr.hci_channel = HCI_CHANNEL_CONTROL;
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ALOGE("bind(HCI_CHANNEL_CONTROL) failed: %s", strerror(errno));
        close(fd);
        return 1;
    }

    // Wait up to 15s for hci0 to finish QCA NVM/firmware setup
    for (int i = 0; i < 30; i++) {
        if (index_in_list(fd, MGMT_OP_READ_INDEX_LIST, 0)) {
            ALOGI("hci0 is already configured");
            close(fd);
            return 0;
        }
        if (index_in_list(fd, MGMT_OP_READ_UNCONF_INDEX_LIST, 0)) {
            uint8_t status = 0xff;
            uint16_t out_len = 0;
            if (send_mgmt_cmd(fd, MGMT_OP_SET_PUBLIC_ADDRESS, 0, bdaddr, 6,
                              &status, nullptr, &out_len) && status == 0) {
                ALOGI("Configured hci0 public BD_ADDR %02x:%02x:%02x:%02x:%02x:%02x",
                      bdaddr[5], bdaddr[4], bdaddr[3], bdaddr[2], bdaddr[1], bdaddr[0]);
                close(fd);
                return 0;
            }
            ALOGW("MGMT_OP_SET_PUBLIC_ADDRESS status=%u, retrying...", status);
        }
        usleep(500000);
    }

    ALOGE("Timed out waiting for hci0");
    close(fd);
    return 1;
}
