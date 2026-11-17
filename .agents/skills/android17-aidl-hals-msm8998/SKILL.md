---
name: android17-aidl-hals-msm8998
description: >-
  Use this skill when designing, implementing, or integrating clean-room Android
  17 AIDL Hardware Abstraction Layers (Radio/QMI, Sensors/SLPI, Audio/QDSP6,
  Camera/libcamera, Graphics/Mesa3D Freedreno/Turnip, Power, Health, Thermal,
  Lights, Vibrator) on the OnePlus 5T (dumpling / MSM8998) running Linux 6.18 LTS.
---

# Android 17 Clean-Room AIDL HAL Implementation Skill (`msm8998`)

This skill documents the Android 17 (LineageOS 24 `lineage_dumpling-cp2a-userdebug`, API 37.0) AIDL HAL integration on the OnePlus 5T running **Linux 6.18 LTS (`6.18.0-msm8998+`, `#62` commit `8f55c2473`)**.

## Core Design & Build Principles

1. **Pure AIDL & APEX HALs:** Android 17 deprecates legacy HIDL (`hwbinder`). All HALs register with `/dev/binder` (`IServiceManager`) as VINTF-declared `@aidl` services.
2. **Upstream Kernel Interfaces Only:** Target standard Linux 6.18 interfaces (`DRM/KMS`, `V4L2`, `ALSA`, `AF_QIPCRTR`, `EV_FF`, `sysfs`), never legacy `/dev/msm_*` or `/dev/kgsl-3d0` ioctls.
3. **LineageOS 24 Build Env on `.15`:** Always `export USE_CCACHE=1 CC_WRAPPER=/usr/bin/ccache` before `source build/envsetup.sh && lunch lineage_dumpling-cp2a-userdebug`, otherwise Soong re-runs full `Android.bp` analysis (~4m45s).

## Subsystem Integration Status

### 1. Graphics, Display & Early GPU Firmware (`IComposer3`, `IAllocator`, `IMapper5`)
- **Implementation:** Prebuilt Mesa3D `freedreno` + `turnip` (`device/oneplus/dumpling/prebuilt/mesa/`) + `minigbm_msm` + `drm_hwcomposer` APEX.
- **Boot Fix:** Copy `a530_pm4.fw`, `a530_pfp.fw`, and `a540_gpmu.fw2` into `$(TARGET_COPY_OUT_RAMDISK)/vendor/firmware/qcom/` in `device.mk` so early `1.4s` kernel probes succeed before `/vendor` mounts.
- **`libmeminfo` / eBPF Crash Fix:** On top of Kernel `#62` enabling `FTRACE`/`KPROBES`/`BTF` for `BpfLoader-rs`, `system/memory/libmeminfo/sysmeminfo.cpp` guards `ReadPerProcessGpuMem` and `ReadProcessGpuUsageKb` with `access("/sys/fs/bpf/map_gpuMem_gpu_mem_total_map", F_OK)` so `BpfMapRO::abortOnMismatch` never `SIGABRT`s `system_server`.

### 2. Audio (`android.hardware.audio.core.IModule` AIDL)
- **Kernel Stack:** `snd-soc-msm8998` (`0 [O5T]`) + `WCD9335` (`divclk1` PM8998 GPIO 13) + `wcd9335-cpe` (`cpe_9335.mdt`) + `TFA9890` (`3-0036`, CoolFlux bypass).
- **HAL & Policy (`device/oneplus/dumpling/configs/audio/` + `rootdir/bin/init.oneplus5t.hw.sh`):**
  - Uses AOSP AIDL `StreamPrimary` ALSA address routing:
    - `Speaker`: `CARD_0_DEV_0` (`MultiMedia1` -> `QUAT_MI2S_RX` `TFA9890`)
    - `Built-In Mic` / `Back Mic` / `Wired Headset Mic`: `CARD_0_DEV_1` (`MultiMedia2` <- `SLIMBUS_0_TX` `ADC2/ADC3`)
    - `Wired Headset` / `Wired Headphones`: `CARD_0_DEV_2` (`MultiMedia3` -> `SLIMBUS_0_RX` `AIF1_PB` -> `RX1/RX2` -> `HPHL/HPHR`)
    - `Earpiece`: `CARD_0_DEV_3` (`MultiMedia4` -> `SLIMBUS_1_RX` `AIF2_PB` -> `RX0` -> `RX INT0` -> `EAR PA`)
  - **Important:** Do **not** include `compressed_offload` in `primary_audio_policy_configuration.xml` unless a real ALSA offload backend is implemented — AOSP `ModulePrimary` routes `COMPRESS_OFFLOAD` to `StreamOutOffloadStub` (silent simulated playback!).

### 3. Wi-Fi & Bluetooth (`WCN3990`)
- **Wi-Fi:** Clean-room `libwifi-hal-ath10k` (`device/oneplus/dumpling/wifi_hal/`, `BOARD_WLAN_DEVICE := ath10k`) statically linked into the Wi-Fi APEX to avoid linker namespace blocks, plus `wlan-mac-qmi` fallback in `init.oneplus5t.hw.sh`.
- **SELinux Rule:** Vendor property `vendor.wlan.firmware.version` must use a `vendor_`-prefixed context type (`vendor_wifi_hal_prop` via `vendor_internal_prop(...)` in `sepolicy/vendor/property.te`) to pass VTS `check_prop_prefix`.
- **Bluetooth:** `oneplus5t-bt-init` (`device/oneplus/dumpling/bt_init/`) reads `/mnt/vendor/persist/bluetooth/.bt_nv.bin` and sets public BD_ADDR via `MGMT_OP_SET_PUBLIC_ADDRESS` (`0x0039`). Label `/dev/rfkill` as `rfkill_device`.

### 4. Sensors, Vibrator, Lights & Keyhandler
- **Sensors (`device/oneplus/dumpling/sensors/`):** `android.hardware.sensors-service.oneplus5t` over `AF_QIPCRTR` `SNS_SMGR_SVC` (`0x100`): `BMI160` accel/gyro, `AK09911`/`AK09915` mag, `PA224` proximity, `TCS3400` ALS + 8 AOSP fused sensors.
- **Vibrator (`device/oneplus/dumpling/vibrator/`):** `android.hardware.vibrator-service.oneplus5t` using `EV_FF` (`EVIOCSFF` `FF_RUMBLE`) on `/dev/input/event*` (`spmi_haptics`). *(Minor TODO: return `EX_UNSUPPORTED_OPERATION` or omit frequency profile inconsistency causing `VibratorInfo` log warning.)*
- **Lights (`hardware/lineage/interfaces/light/`):** Patched `Devices.cpp` + `LedDevice.{h,cpp}` to recognize `"rgb:status"` and write `"R G B"` to `/sys/class/leds/rgb:status/multi_intensity` alongside `brightness` (`0664` in `init.qcom.rc`).
- **Keyhandler (`device/oneplus/dumpling/keyhandler/`):** `com.oneplus5t.keyhandler` handles tri-state alert slider (`KEY_F1..F3`) and `Synaptics s3508` DT2W (`KEY_WAKEUP`).

### 5. Deferred / Future Work Subsystems
1. **Baseband Telephony (`IRadio*` AIDL):** `hals/radio/oneplus5t_ril_qmi.cpp` + `oneplus5t-ril-qmi` currently validate QMI/QRTR (`DMS`/`NAS`/`UIM` USIM detection), and `ipa` creates `rmnet_ipa0`. Needs full 7-interface Android 17 `IRadio` AIDL service (`Network`, `Data`, `Voice`, `Sim`, `Messaging`, `Modem`, `Config`) + data call `rmnet` setup.
2. **Camera (`ICameraProvider` + `libcamera`):** Mainline `camss` (`imx398`, `imx376k`, `imx371`, `lc898217xc` VCM) streams RAW10 (`SRGGB10P`) via V4L2/MediaController, **but** LineageOS 24's bundled `external/libcamera` lacks `src/libcamera/software_isp/`. Future work needs upstream `libcamera` with Simple Pipeline / SoftISP + `ICameraProvider` AIDL HAL.
3. **NFC (`INfc`) & Goodix Fingerprint (`IFingerprint`):** PN553 `nfc0` is bound in kernel (`AF_NFC`) and needs an AIDL `INfc` netlink bridge; Goodix `gfp5288` needs an `libion -> /dev/dma_heap/cma` shim + `/dev/qseecom` AIDL wrapper (`IKeyMintDevice`, `IGatekeeper`, `IFingerprint`) instead of software/virtual fallbacks.
4. **SELinux Enforcing:** Boot currently permissive (`androidboot.selinux=permissive`); remaining non-shell AVCs (`audioserver tmpfs:file`, `system_suspend sysfs:file`, `diag_router`/`tqftpserv`/`rmtfs` `qipcrtr_socket`) must be cleaned up before switching to enforcing.
