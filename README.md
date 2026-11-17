# OnePlus 5T (`dumpling` / MSM8998) — Android 17 (LineageOS 24) ROM, HALs, Findings & Skills

Separate ROM/HALs/knowledge repository paired with the **Linux 6.18 LTS Kernel** repository (`linux-mainline-6.18`, branch `dumpling-6.18-android17`, HEAD `#62` `8f55c2473` / docs `452cea89b`).

> **Start Here When Resuming:** Read [`docs/00_START_HERE_FUTURE_PICKUP.md`](docs/00_START_HERE_FUTURE_PICKUP.md) first.

## Repository Contents

- [`device/oneplus/dumpling/`](device/oneplus/dumpling/) — LineageOS 24 / Android 17 (`lineage_dumpling-cp2a-userdebug`, API 37.0) device tree:
  - OnePlus 5T AIDL HALs (`wifi_hal/`, `bt_init/`, `sensors/`, `vibrator/`, `keyhandler/`)
  - Audio policy (`configs/audio/primary_audio_policy_configuration.xml` wiring Speaker `MM1`, Mics `MM2`, Headphones `MM3`, Earpiece `MM4`, without `compressed_offload` stub)
  - SELinux vendor policy (`sepolicy/vendor/`) passing `selinux_policy` & VTS property namespace checks
  - Hardware init (`rootdir/bin/init.oneplus5t.hw.sh`, `rootdir/etc/init.qcom.rc`)
  - Prebuilts (`prebuilt/kernel/` Kernel `#62` `Image.gz-dtb` + 12 stripped modules, Mesa3D `freedreno`/`turnip`, Qualcomm firmware blobs, QMI daemons)
- [`patches/`](patches/) — Framework & LineageOS interface patches (`system/memory/libmeminfo` eBPF guard, `hardware/lineage/interfaces` `rgb:status` Light HAL, `dumpling.xml` local manifest)
- [`hals/`](hals/) — Clean-room standalone QMI & sysfs HAL prototypes (`radio/`, `sensors/`, `sysfs_hals/`)
- [`userspace/`](userspace/) — Clean-room C/C++ sources for `qrtr`, `rmtfs`, `tqftpserv`, `pd-mapper`, `diag-router`, `sns_reg2`, `wlan_mac_qmi`, `pil-squasher`, `qce_test`, `track1_test`
- [`diagnostics/`](diagnostics/) — LineageOS 24 Boot `#0` diagnostics (`dmesg_boot0.txt`, `logcat_boot0.txt`, `avc_dmesg.txt`, `service_list.txt`, `asound.txt`, `dumpsys_*`) plus stock `4.4.205` `stock_defconfig`, `stock_02.dts`, and clean-room REA specs
- [`docs/`](docs/) — Complete documentation set (`00`–`11`) covering stock kernel vs mainline `6.18`, Boot `#0` diagnostics & fixes, firmware/security/QSEE, and test plans
- [`.agents/skills/`](.agents/skills/) — Operational skills (`android17-aidl-hals-msm8998`, `msm8998-mainline-android-kernel`, `oneplus5t-backup-restore`, `qcom-rea-cleanroom`)

## Build Resume Summary

```bash
cd ~/oneplus5t-android17/lineage-24
export USE_CCACHE=1
export CC_WRAPPER=/usr/bin/ccache
source build/envsetup.sh
lunch lineage_dumpling-cp2a-userdebug
m -j16 bacon bootimage systemimage vendorimage recoveryimage
```
