---
name: oneplus5t-backup-restore
description: >-
  Use this skill when unlocking the bootloader, booting temporary RAM recovery,
  backing up raw UFS partitions and GPT headers, or restoring critical EFS/radio
  and sensor calibration partitions (persist, modemst1, modemst2, fsg, oem_dycnvbk,
  oem_stanvbk, param) on the OnePlus 5T (dumpling / MSM8998).
---

# OnePlus 5T (`dumpling`) Partition Backup & EFS/Calibration Restore Skill

This skill provides the verified workflow and scripts to back up all 67 non-userdata UFS partitions across LUN 0–5 (`sda`–`sdf`) plus raw GPT headers on the OnePlus 5T (`ONEPLUS_A5010`), and to safely restore device-unique calibration or radio partitions if ever needed.

## Prerequisites & Host Access Contract

- **USB Lab Host (`192.168.1.87`):** Holds attached OnePlus 5T (`5f5440e4`), `adb`/`fastboot`, udev rules (`2a70` / `18d1`), and the verified backup archive (`/home/zero/oneplus5t-android17/backups/`).
- **Build & Archive Host (`192.168.1.15`):** Xeon build server (`~/oneplus5t-android17/backups/` holds the verified full 78-entry SHA-256 copy including `system_verity_clean.img`, repaired via AOSP `fec -d` against Reed-Solomon parity to pass `verity_verifier` cleanly).
- **SSH Without Global Config:**
  `SSH_OPTS="-i /home/zero/.gemini/antigravity/scratch/oneplus5t/id_ed25519 -o UserKnownHostsFile=/home/zero/.gemini/antigravity/scratch/oneplus5t/known_hosts -o StrictHostKeyChecking=yes"`
- **Recovery Image:** `twrp-3.7.1_12-2-cheeseburger_dumpling.img` (also works via `fastboot boot` non-destructively in RAM).
- **Important TWRP / LineageOS GApps Note:** Official LineageOS 22.2 `lineage-22.2-20261110-nightly-dumpling-signed.zip` `backuptool.sh` may not preserve `MindTheGapps-15.0.0-arm64` across dirty/re-flashes — always re-flash `MindTheGapps` right after flashing/re-installing the ROM zip if GApps are used. When reverting `/data` from F2FS (Android 17) back to LineageOS 22.2 / OOS, format `userdata` and `cache` cleanly (`ext4` `-b 4096 -O quota,project`).

## Workflow Steps

### 1. Boot Temporary Recovery in RAM (Non-Destructive)
```bash
fastboot boot /home/zero/oneplus5t-android17/backups/twrp-3.7.1_12-2-cheeseburger_dumpling.img
```
Wait ~12 seconds for USB PID `18d1:2d08`, then verify `adb devices -l` and `adb shell id`.

### 2. Run Full Partition & GPT Backup
Execute [backup_partitions.sh](./scripts/backup_partitions.sh) on the USB host and verify `SHA256SUMS.txt` (`chmod 0444` on `critical_unique/`).

### 3. Emergency EFS / Calibration Restoration
If radio EFS (`modemst1`, `modemst2`, `fsg`, `fsc`) or sensor/camera/Wi-Fi factory calibration (`persist`, `oem_dycnvbk`, `oem_stanvbk`, `param`) ever needs restoration, boot into RAM recovery and run [restore_critical_efs.sh](./scripts/restore_critical_efs.sh).

### 4. End-of-Development Bootloader Relock Safety (`YELLOW` Verified Boot v1.0)
- **Keep UNLOCKED during all bring-up:** `abl.img` (`0x2dc94.c`) blocks `fastboot boot` and `fastboot flash` when locked, wipes `userdata` on unlock/lock, and bricks to **EDL (`05c6:9008`)** if both `boot` and `recovery` enter `RED` state.
- **Only relock on official unmodified stock OOS (`system_verity_clean.img` + stock `boot`/`recovery`/`vendor` matching OEM key) OR custom-signed `YELLOW` VB 1.0 (`boot_signer` on both `boot` and `recovery` with verified `dm-verity`)**: never run `fastboot oem lock` with unsigned/custom recovery or non-stock `boot`/`system`. See `docs/02_signed_firmware_and_security.md` and [revert_to_stock_oos.sh](./scripts/revert_to_stock_oos.sh).

## References
- Full UFS LUN 0–5 partition layout and sizes: [partition_table.md](./references/partition_table.md)
