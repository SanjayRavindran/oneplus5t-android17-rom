/*
 * Clean-Room QMI DMS (0x02) GET_MAC_ADDRESS (0x005C) client for OnePlus 5T
 * Queries factory WLAN MAC over AF_QIPCRTR when /persist/wlan_mac.bin is empty.
 */
#include <errno.h>
#include <linux/qrtr.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define QMI_SVC_DMS               0x0002
#define QMI_DMS_GET_MAC_ADDR_REQ  0x005C
#define QMI_DMS_DEV_WLAN          0

struct __attribute__((packed)) qmi_hdr {
	uint8_t  ctl_flags;
	uint16_t txn_id;
	uint16_t msg_id;
	uint16_t msg_len;
};

struct __attribute__((packed)) dms_get_mac_req {
	struct qmi_hdr hdr;
	uint8_t  tlv_type;
	uint16_t tlv_len;
	uint32_t device;
};

static int lookup_dms(uint32_t *out_node, uint32_t *out_port, int timeout_ms)
{
	int sock = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket(AF_QIPCRTR)");
		return -1;
	}

	struct sockaddr_qrtr sq = {0};
	socklen_t slen = sizeof(sq);
	if (getsockname(sock, (struct sockaddr *)&sq, &slen) < 0) {
		perror("getsockname");
		close(sock);
		return -1;
	}

	struct qrtr_ctrl_pkt pkt = {0};
	pkt.cmd = QRTR_TYPE_NEW_LOOKUP;
	pkt.server.service = QMI_SVC_DMS;

	struct sockaddr_qrtr ns = {
		.sq_family = AF_QIPCRTR,
		.sq_node = sq.sq_node,
		.sq_port = QRTR_PORT_CTRL,
	};
	if (sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&ns, sizeof(ns)) < 0) {
		perror("sendto(NEW_LOOKUP)");
		close(sock);
		return -1;
	}

	struct pollfd pfd = { .fd = sock, .events = POLLIN };
	int elapsed = 0;
	while (elapsed < timeout_ms) {
		int rc = poll(&pfd, 1, 200);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (rc == 0) {
			elapsed += 200;
			continue;
		}
		struct qrtr_ctrl_pkt resp = {0};
		ssize_t n = recvfrom(sock, &resp, sizeof(resp), 0, NULL, NULL);
		if (n >= (ssize_t)sizeof(resp) && resp.cmd == QRTR_TYPE_NEW_SERVER) {
			if (resp.server.service == 0 && resp.server.node == 0 && resp.server.port == 0) {
				elapsed += 200;
				usleep(200000);
				sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&ns, sizeof(ns));
				continue;
			}
			if (resp.server.service == QMI_SVC_DMS) {
				*out_node = resp.server.node;
				*out_port = resp.server.port;
				close(sock);
				return 0;
			}
		}
	}
	close(sock);
	return -1;
}

int main(int argc, char **argv)
{
	uint32_t dev_type = QMI_DMS_DEV_WLAN;
	if (argc > 1)
		dev_type = (uint32_t)strtoul(argv[1], NULL, 0);

	uint32_t node = 0, port = 0;
	if (lookup_dms(&node, &port, 5000) < 0) {
		fprintf(stderr, "wlan-mac-qmi: QMI DMS service not found on QRTR\n");
		return 1;
	}

	int sock = socket(AF_QIPCRTR, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}

	struct sockaddr_qrtr dst = {
		.sq_family = AF_QIPCRTR,
		.sq_node = node,
		.sq_port = port,
	};
	struct dms_get_mac_req req = {
		.hdr = {
			.ctl_flags = 0x00,
			.txn_id = 1,
			.msg_id = QMI_DMS_GET_MAC_ADDR_REQ,
			.msg_len = 7,
		},
		.tlv_type = 0x01,
		.tlv_len = 4,
		.device = dev_type,
	};

	if (sendto(sock, &req, sizeof(req), 0, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
		perror("sendto(DMS_GET_MAC_ADDRESS)");
		close(sock);
		return 1;
	}

	uint8_t buf[512];
	struct pollfd pfd = { .fd = sock, .events = POLLIN };
	for (int i = 0; i < 15; i++) {
		int rc = poll(&pfd, 1, 200);
		if (rc <= 0)
			continue;
		ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
		if (n < (ssize_t)sizeof(struct qmi_hdr))
			continue;
		const struct qmi_hdr *h = (const struct qmi_hdr *)buf;
		if (h->ctl_flags != 0x02 || h->txn_id != 1 || h->msg_id != QMI_DMS_GET_MAC_ADDR_REQ)
			continue;

		size_t off = sizeof(struct qmi_hdr);
		uint16_t res_code = 0xffff, err_code = 0xffff;
		const uint8_t *mac = NULL;
		while (off + 3 <= (size_t)n) {
			uint8_t t = buf[off];
			uint16_t l = (uint16_t)buf[off + 1] | ((uint16_t)buf[off + 2] << 8);
			off += 3;
			if (off + l > (size_t)n)
				break;
			if (t == 0x02 && l >= 4) {
				res_code = (uint16_t)buf[off] | ((uint16_t)buf[off + 1] << 8);
				err_code = (uint16_t)buf[off + 2] | ((uint16_t)buf[off + 3] << 8);
			} else if (t == 0x10) {
				if (l == 7 && buf[off] == 6)
					mac = buf + off + 1;
				else if (l == 6)
					mac = buf + off;
				else
					fprintf(stderr, "wlan-mac-qmi: unexpected TLV 0x10 len=%u\n", l);
			}
			off += l;
		}
		close(sock);
		if (res_code != 0) {
			fprintf(stderr, "wlan-mac-qmi: DMS error res=%u err=%u\n", res_code, err_code);
			return 1;
		}
		if (!mac || (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0 || (mac[0] & 1)) {
			fprintf(stderr, "wlan-mac-qmi: invalid MAC in response\n");
			return 1;
		}
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
		       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		return 0;
	}
	close(sock);
	fprintf(stderr, "wlan-mac-qmi: timeout waiting for DMS response\n");
	return 1;
}
