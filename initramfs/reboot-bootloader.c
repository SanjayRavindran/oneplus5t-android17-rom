/*
 * Clean-Room Unattended Fastboot Reboot Utility for OnePlus 5T (MSM8998)
 * Triggers Linux kernel reboot syscall with LINUX_REBOOT_CMD_RESTART2 ("bootloader"),
 * which programs PM8998 PON_SOFT_RB_SPARE (0x88f) = 0x2 via qcom-pon driver.
 * License: GPL-2.0-only
 */
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/reboot.h>

int main(void) {
    sync();
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
            LINUX_REBOOT_CMD_RESTART2, "bootloader");
    return 0;
}
