# OnePlus 5T (`dumpling` / MSM8998) — Future Pickup & Resume Guide

**Last Updated:** 2026-11-17
**Target Hardware:** OnePlus 5T (`ONEPLUS A5010`, `17801`, `hw_version=43`, MSM8998 v2.1)

This document is the single entry point to resume Android 17 (LineageOS 24) + Mainline Linux `6.18 LTS` development cleanly without re-discovering hardware quirks, build flags, or diagnostics.

---

## 1. Two-Repository Layout

1. **Kernel Repository (`linux-mainline-6.18`, branch `dumpling-6.18-android17`)**
   - Location on build host (`.15`): `~/oneplus5t-android17/kernel/linux-mainline-6.18`
   - Contains: Upstream `6.18.0-msm8998` base (`upstream-6.18`) + full OnePlus 5T mainline port through **Kernel `#62` (`8f55c2473`)**, `docs/` (stock `4.4.205` vs `6.18` deep technical findings & build guide), and `skills/msm8998-mainline-android-kernel/`.
2. **ROM, HALs, Patches, Diagnostics & Skills Repository (`oneplus5t-android17-rom`, branch `main`)**
   - Location on `.15`: `~/oneplus5t-android17/oneplus5t-android17-rom`
   - Mirrored in workspace: `oneplus5t-android17-rom/`
   - Contains:
     - `device/oneplus/dumpling/` — Complete LineageOS 24 / Android 17 device tree (`BoardConfig.mk`, `device.mk`, audio configs, `sepolicy/vendor/`, `rootdir/`, custom AIDL HALs for Wi-Fi/BT/Sensors/Vibrator/Keyhandler, Mesa3D `freedreno`/`turnip` prebuilts, Kernel `#62` + stripped modules, and QMI/firmware blobs).
     - `patches/` — Git patches for `system/memory/libmeminfo` (`sysmeminfo.cpp`), `hardware/lineage/interfaces` (`light/` `rgb:status`), and `.repo/local_manifests/dumpling.xml` (`vendor_gms`).
     - `hals/` — Clean-room standalone QMI/sysfs HAL prototype sources (`radio/`, `sensors/`, `sysfs_hals/`).
     - `userspace/` — Clean-room userspace source trees (`qrtr`, `rmtfs`, `tqftpserv`, `pd-mapper`, `pil-squasher`, `diag`, `sns_reg2`, `wlan_mac_qmi`, `qce_test`, `track1_test`).
     - `diagnostics/lineage24_boot0/` & `diagnostics/stock_and_rea/` — Boot `#0` `dmesg`/`logcat`/SELinux `avc`/service dumps plus `stock_defconfig`, `stock_02.dts`, and clean-room REA specs.
     - `docs/` (`00`–`11`) — Full knowledge base covering both stock `4.4.205` and mainline `6.18` + LineageOS 24.
     - `.agents/skills/` — All 4 operational Antigravity skills for kernel, AIDL HALs, REA clean-room analysis, and UFS backup/restore.

---

## 2. Current Checkpoint & Unflashed Fixes Already Ready in Git

### Already Built & Live-Tested Earlier (`#58` / Evolution X & Boot `#0` LineageOS 24)
- Display (`DPU 3.0` + `S6E3FC1`), Adreno 540 GPU (`freedreno`/`minigbm`/`drm_hwcomposer`), Touch (`Synaptics s3508` + DT2W), Wi-Fi (`ath10k_snoc` + static `libwifi-hal-ath10k`), Bluetooth (`hci0` + `oneplus5t-bt-init`), SLPI Sensors (`BMI160`/`AK09911`/`PA224`/`TCS3400`), Vibrator (`spmi_haptics` `EV_FF`), `bq27411` battery + thermal zones, Triple Camera raw V4L2 (`camss` + `sony-imx-op5t`), Venus codecs, `/dev/qseecom` + RPMB + `goodix_op5t` kernel side, `oneplus_dash` STM8S kernel driver, and `O5T` ALSA card (`WCD9335` + `cpe_9335.mdt` + `TFA9890`).

### Fixed in Code After LineageOS 24 Boot `#0`, **Pending Next Full Build & Flash**
1. **Kernel `#62` (`8f55c2473`):** Enabled `CONFIG_FTRACE`, `CONFIG_TRACING`, `CONFIG_EVENT_TRACING`, `CONFIG_FUNCTION_TRACER`, `CONFIG_DYNAMIC_FTRACE`, `CONFIG_KPROBES`, `CONFIG_KPROBE_EVENTS`, `CONFIG_UPROBE_EVENTS`, `CONFIG_BPF_EVENTS`, `CONFIG_DEBUG_INFO_DWARF5`, `CONFIG_DEBUG_INFO_BTF` (verified `.BTF`/`.BTF_ids` in `vmlinux`) so Android 17 `BpfLoader-rs` registers tracepoint/kprobe/GPU/timeInState/dmabuf BPF programs cleanly.
2. **Userspace `libmeminfo` Crash Guard (`patches/system_memory_libmeminfo/`):** Guarded `ReadPerProcessGpuMem` and `ReadProcessGpuUsageKb` with `access("/sys/fs/bpf/map_gpuMem_gpu_mem_total_map", F_OK)` so `system_server` (`ActivityManager`) never hits `BpfMapRO::abortOnMismatch` `SIGABRT` if that BPF map is ever absent.
3. **OnePlus 5T Audio Routing & Offload Fix (`device/oneplus/dumpling/configs/audio/` + `init.oneplus5t.hw.sh`):**
   - Replaced Cuttlefish XML with OnePlus 5T `primary_audio_policy_configuration.xml` wiring `Speaker` (`CARD_0_DEV_0`), `Built-In Mic`/`Back Mic`/`Headset Mic` (`CARD_0_DEV_1`), `Wired Headset`/`Headphones` (`CARD_0_DEV_2`), and `Earpiece` (`CARD_0_DEV_3`).
   - Removed `compressed_offload` so AOSP `ModulePrimary` does **not** route MP3/AAC playback to silent `StreamOutOffloadStub`.
   - Fixed syntax error in `init.oneplus5t.hw.sh` and added ALSA mixer setup for `SLIMBUS_1_RX` (`Earpiece`), `SLIMBUS_0_RX` (`Headphones`), `QUAT_MI2S_RX` (`Speaker`), and `SLIMBUS_0_TX` (`ADC2/3` mics).
4. **Notification RGB LED (`patches/hardware_lineage_interfaces/` + `init.qcom.rc`):** Added `"rgb:status"` + `multi_intensity` `"R G B"` sysfs writes to `android.hardware.light-service.lineage`.
5. **Early GPU Firmware in Ramdisk (`device.mk`):** Copied `a530_pm4.fw`, `a530_pfp.fw`, and `a540_gpmu.fw2` into `$(TARGET_COPY_OUT_RAMDISK)/vendor/firmware/qcom/` to avoid `-ENOENT` during early `1.4s` kernel probe before `/vendor` mounts.
6. **SELinux Policy (`device/oneplus/dumpling/sepolicy/vendor/`):** Verified `m selinux_policy android.hardware.light-service.lineage` passes cleanly (`vendor_wifi_hal_prop` namespace prefix, `rfkill_device`, `rmtfs` `block_device:blk_file getattr`, HAL rules).

---

## 3. Exact Build Resume Commands (on `.15`)

> **Important:** `~/oneplus5t-android17/lineage-24` uses git object alternates pointing at `~/oneplus5t-android17/evolution-cnb/.repo` — **NEVER delete `evolution-cnb/.repo`!**

```bash
cd ~/oneplus5t-android17/lineage-24
export USE_CCACHE=1
export CC_WRAPPER=/usr/bin/ccache
source build/envsetup.sh
lunch lineage_dumpling-cp2a-userdebug
m -j16 bacon bootimage systemimage vendorimage recoveryimage
```
*Tip:* Always set `USE_CCACHE=1 CC_WRAPPER=/usr/bin/ccache` before running `m`, otherwise Soong re-runs ~4m45s of full `Android.bp` analysis.

---

## 4. Remaining Work Queue When Resuming

1. **Finish & Flash Second LineageOS 24 Build:** Complete `m -j16 bacon bootimage systemimage vendorimage recoveryimage`, verify `dmesg` / `logcat` / `BpfLoader` / `system_server` stability, and test Earpiece, Speaker, Wired Headphones, Mic, and `rgb:status` LED live.
2. **Telephony (`IRadio*` AIDL):** Upgrade `oneplus5t-ril-qmi` from a QMI probe daemon into the 7-interface Android 17 `IRadio` AIDL service (`Sim`, `Network`, `Voice`, `Data`, `Messaging`, `Modem`, `Config`) + `rmnet_ipa0` data call bring-up.
3. **Camera (`ICameraProvider` + `libcamera` SoftISP):** Port upstream `libcamera` with `src/libcamera/software_isp/` (absent in bundled LineageOS 24 `external/libcamera`) + AOSP Camera AIDL HAL targeting `camss` `RDI0..2` (`IMX398`/`IMX376K`/`IMX371`).
4. **NFC, Fingerprint (`qseecom`), KeyMint & SELinux Enforcing:** Add `AF_NFC` netlink `INfc` AIDL HAL; add `libion -> /dev/dma_heap/cma` + `/dev/qseecom` AIDL wrapper for Goodix `gfp5288` (`0x111`); clean up remaining non-shell AVCs (`audioserver tmpfs`, `system_suspend sysfs`, `qrtr` sockets) before removing `androidboot.selinux=permissive`; fix `VibratorInfo` resonant frequency metadata warning (`vibrator/Vibrator.cpp`).

---

## 5. Archive & Daily-Driver State

- **Backups on `.15` (`~/oneplus5t-android17/backups/`):** All 78 entries pass `sha256sum -c SHA256SUMS.txt`, including mathematically FEC-repaired stock `system_verity_clean.img` (`54dd4b64...`, `verity_verifier`: `VERIFIED`).
- **Daily Driver (`5f5440e4`):** Kept separate from Android 17 build tree on `.87` with TWRP `3.7.1_12-2`, official LineageOS 22.2 (`22.2-20261110-NIGHTLY-dumpling`), MindTheGapps 15, and Magisk v31.1 (`ext4` `/data`).
