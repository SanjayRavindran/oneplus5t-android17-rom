# OnePlus 5T (`ONEPLUS_A5010` / `dumpling`) UFS Partition Reference

Captured from `fastboot getvar all` and `/dev/block/bootdevice/by-name`:

## LUN 0 (`/dev/block/sda`)
- `sda1` (`ssd`): `0x2000` (8 KB) — Secure Software Download cookie
- `sda2` (`persist`): `0x2000000` (32 MB, `ext4`) — Factory calibration (Wi-Fi MAC, BT NV, Dual-Camera stereo OTP, SLPI sensor registry, Goodix FP)
- `sda3` (`cache`): `0x10000000` (256 MB, `ext4`) — Cache partition
- `sda4` (`misc`): `0x100000` (1 MB) — Bootloader Control Block (BCB)
- `sda5` (`keystore`): `0x80000` (512 KB) — Keystore
- `sda6` (`frp`): `0x80000` (512 KB) — Factory Reset Protection
- `sda7` (`oem_dycnvbk`): `0xA00000` (10 MB) — OnePlus Dynamic RF NV Backup
- `sda8` (`oem_stanvbk`): `0xA00000` (10 MB) — OnePlus Static RF NV Backup
- `sda9` (`param`): `0x100000` (1 MB) — OnePlus device parameters, serial, PCB version
- `sda10` (`reserve1`): `0x7E8000` (~7.9 MB)
- `sda11` (`reserve2`): `0xFD0000` (~15.8 MB)
- `sda12` (`config`): `0x80000` (512 KB)
- `sda13` (`userdata`): `0x1C4D8BB000` (~113.2 GB, `ext4`/`f2fs`)

## LUN 1 & LUN 2 (`/dev/block/sdb`, `/dev/block/sdc`)
- `sdb1` (`xbl`): `0x3F5000` (~3.95 MB) — Primary eXtensible Bootloader
- `sdc1` (`xblbak`): `0x3F5000` (~3.95 MB) — Backup eXtensible Bootloader

## LUN 3 (`/dev/block/sdd`)
- `sdd1` (`reserve`): `0x8000` (32 KB)
- `sdd2` (`cdt`): `0x1000` (4 KB) — Config Data Table (SoC/board ID)
- `sdd3` (`ddr`): `0x100000` (1 MB) — LPDDR4X training parameters

## LUN 4 (`/dev/block/sde`)
- `sde1`/`sde2` (`rpm`/`rpmbak`): `0x80000` (512 KB) — Resource Power Manager firmware
- `sde3`/`sde4` (`tz`/`tzbak`): `0x200000` (2 MB) — Qualcomm TrustZone (`QSEE`)
- `sde5`/`sde6` (`hyp`/`hypbak`): `0x80000` (512 KB) — Hypervisor
- `sde7` (`sec`): `0x4000` (16 KB) — Security fuse config
- `sde8`/`sde9` (`pmic`/`pmicbak`): `0x80000` (512 KB) — PM8998/PMI8998 PMIC firmware
- `sde10` (`modem`): `0x6E00000` (110 MB, `vfat`) — Baseband MPSS, ADSP, SLPI, Venus, IPA, WLAN firmware
- `sde11` (`dsp`): `0x1000000` (16 MB, `ext4`) — Hexagon DSP runtime libraries
- `sde16`/`sde17` (`abl`/`ablbak`): `0x800000` (8 MB) — Android Bootloader (UEFI LinuxLoader)
- `sde18` (`LOGO`): `0x1000000` (16 MB) — Boot logo
- `sde19` (`boot`): `0x4000000` (64 MB) — Kernel + Initramfs
- `sde20` (`boot_aging`): `0x4000000` (64 MB)
- `sde21` (`system`): `0xC0000000` (3072 MB, `ext4`) — System partition
- `sde22` (`recovery`): `0x4000000` (64 MB) — Recovery partition
- `sde23` (`devinfo`): `0x1000` (4 KB) — Bootloader unlock state
- `sde24` (`bluetooth`): `0x100000` (1 MB, `vfat`) — WCN3990 Bluetooth firmware
- `sde25`/`sde26` (`keymaster`/`keymasterbak`): `0x80000` (512 KB)
- `sde27`–`sde30` (`cmnlib`/`cmnlib64` + `bak`): `0x80000` (512 KB each)
- `sde39` (`devcfg`): `0x20000` (128 KB) — TrustZone Device Configuration
- `sde43`–`sde46` (`fw_4j1ed`, `fw_8v1ee`, `fw_4g9n4`, `fw_4t0n8`): `0x200000` (2 MB each) — UFS flash vendor microcode images (`UFSH` magic on first 3; `fw_4t0n8` zeroed)

## LUN 5 (`/dev/block/sdf`)
- `sdf1` (`modemst1`): `0x200000` (2 MB) — Live Modem EFS 1 (IMEI & RF calibration)
- `sdf2` (`modemst2`): `0x200000` (2 MB) — Live Modem EFS 2 (IMEI & RF calibration)
- `sdf3` (`fsg`): `0x200000` (2 MB) — Golden Factory Modem EFS backup
- `sdf4` (`fsc`): `0x1000` (4 KB) — Modem EFS cookie
- `sdf5` (`md5`): `0x80000` (512 KB)
- `sdf6` (`vendor`): `0x40000000` (1024 MB, `ext4`) — Vendor partition
- `sdf7` (`reserve3`): `0x1F958000` (~505 MB)
