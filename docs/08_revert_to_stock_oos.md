# Revert Back to Stock OxygenOS (`dumpling` / OnePlus 5T)

This guide restores the backed-up **stock OxygenOS 10 (`QKQ1.191014.012`)** partitions archived on `.87` under `~/oneplus5t-android17/backups/` using `revert_to_stock_oos.sh`.

> [!CAUTION]
> - Keep the bootloader **unlocked** during and after reverting unless you explicitly re-verify DM-Verity / AVB 1.0 stock state first (`fastboot oem lock` bricks if stock partitions do not match signed hashes).
> - Never erase or overwrite `critical_unique/` partitions (`persist`, `modemst1`, `modemst2`, `fsg`, `param`, `oem_dycnvbk`, `oem_stanvbk`) during a normal OS revert. If calibration/IMEI ever breaks separately, use `restore_critical_efs.sh` in RAM-booted TWRP.

---

## 1. Verified Backup Inventory (`.87`: `~/oneplus5t-android17/backups/`)

All 78 entries in `SHA256SUMS.txt` are verified (`OK`):
- `os_and_firmware/`:
  - `system.img` (`3.0G`, `3,221,225,472` B — flashed in `512M` sparse chunks because ABL `max-download-size` is `768 MiB`)
  - `vendor.img` (`1.0G`, `1,073,741,824` B — flashed in `512M` sparse chunks)
  - `boot.img` (`64M`), `recovery.img` (`64M`), `cache.img` (`256M`), `boot_aging.img` (`64M`)
  - `modem.img` (`110M`), `dsp.img` (`16M`), `bluetooth.img` (`1.0M`), `logdump.img` (`64M`), `minidump.img` (`506M`)
  - `userdata_decrypted.tar.gz` (`987M`, decrypted `/data` + `/sdcard` archive)
- `bootchain_fw/` (`141M`): `xbl*`, `rpm*`, `tz*`, `hyp*`, `pmic*`, `abl*`, `keymaster*`, `cmnlib*`, `mdtp*`, `devcfg`, `storsec*`, `LOGO`, `splash`, `fw_*`
- `critical_unique/` (`597M`): `persist`, `modemst1/2`, `fsg`, `fsc`, `param`, `oem_*`, `misc`, `frp`, `keystore`, `gpt_sd{a..f}_head.bin`

---

## 2. How to Revert (from `.87`)

### Step 1 — Boot Phone into Fastboot Mode
Hold **Volume Up + Power** (or from Evolution X Recovery: **Advanced → Reboot to fastboot**), then confirm connection:
```bash
fastboot devices
```

### Step 2 — Run Dry-Run Check First (Recommended)
Verifies SHA-256 checksums of all images before touching the phone:
```bash
~/oneplus5t-android17/backups/revert_to_stock_oos.sh --dry-run
```

### Step 3 — Revert to Stock OxygenOS

- **Standard Clean Revert** (flashes stock `boot`, `recovery`, `cache`, `vendor`, `system`, formats `userdata` cleanly back from Android 17 `f2fs` + `/metadata`, and reboots into OxygenOS):
  ```bash
  ~/oneplus5t-android17/backups/revert_to_stock_oos.sh
  ```
- **Revert + Restore Backed-Up `/data` & `/sdcard` (`userdata_decrypted.tar.gz`)**:
  *(Also RAM-boots `twrp-3.7.1_12-2-cheeseburger_dumpling.img` after `fastboot -w` and extracts `/data`)*:
  ```bash
  ~/oneplus5t-android17/backups/revert_to_stock_oos.sh --with-data
  ```
- **Revert Including Stock Firmware Partitions (`modem`, `dsp`, `bluetooth`, `boot_aging`, `logdump`)**:
  ```bash
  ~/oneplus5t-android17/backups/revert_to_stock_oos.sh --with-firmware
  ```

---

## 3. Manual Commands (Equivalent)

```bash
cd ~/oneplus5t-android17/backups/os_and_firmware

# Verify checksums first
(cd .. && sha256sum -c SHA256SUMS.txt)

# Flash OS partitions (-S 512M respects OnePlus 5T 768 MiB fastboot max-download-size)
fastboot flash boot boot.img
fastboot flash recovery recovery.img
fastboot flash cache cache.img
fastboot -S 512M flash vendor vendor.img
fastboot -S 512M flash system system.img

# Wipe userdata back cleanly from Android 17 F2FS/FBE and restore stock ext4 cache
fastboot -w
fastboot flash cache cache.img
fastboot reboot
```

*(Optional manual `/data` restore after rebooting into RAM TWRP instead of `fastboot reboot`:)*
```bash
fastboot boot ~/oneplus5t-android17/backups/twrp-3.7.1_12-2-cheeseburger_dumpling.img
# Wait ~12s for ADB recovery, then:
gzip -dc ~/oneplus5t-android17/backups/os_and_firmware/userdata_decrypted.tar.gz | adb exec-in "tar -xpf - -C /data"
adb shell sync
adb reboot
```
