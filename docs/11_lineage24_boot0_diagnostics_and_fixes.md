# LineageOS 24 (Android 17) Boot #0 Diagnostics & Fixes Applied

This document preserves the exact findings from the first live boot of **LineageOS 24 (`lineage_dumpling-cp2a-userdebug`, API 37.0, Kernel `#61`)** collected under `diagnostics/lineage24_boot0/` and the subsequent fixes applied before stopping the build on 2026-11-17.

---

## 1. Root Cause of `system_server` Crash Loop (Boot #0)

1. **`logcat_boot0.txt` (`Fatal signal 6 (SIGABRT)` in `system_server` `ActivityManager`):**
   - `AppProfiler$CachedAppsWatermarkData.updateCachedAppsSnapshot` -> `MemInfoReader.getKernelUsedSizeKb` -> `MemInfoReader.getGpuKernelUsedSizeKb` -> `android::meminfo::ReadProcessGpuUsageKb` -> `android::bpf::BpfMapRO<uint64_t, uint64_t>::abortOnMismatch(bool)` -> `abort()`.
2. **`dmesg_boot0.txt` (`bpfloader` / `NetBpfLoad`):**
   - `bpfloader.rs` failed to load:
     - `timeInState.bpf` (`os error 3`)
     - `gpuMem.bpf` (`os error 22`)
     - `gpuWork.bpf` (`os error 22`)
     - `bpfMemEvents.bpf` (`os error 22`)
     - `kernelWakelockDuration.bpf` (`os error 22`)
     - `dmabufIter.bpf` (`os error 3`)
     - `fuseMedia.bpf` (`os error 2` — mainline `6.18` lacks out-of-tree Android `CONFIG_FUSE_BPF`)
   - Root cause in Kernel `#61` (`kernel_config.txt`): `# CONFIG_FTRACE is not set`, `# CONFIG_KPROBES is not set`, and `CONFIG_DEBUG_INFO_NONE=y` (no `.BTF` section in `vmlinux`), preventing `tracefs` mount and tracepoint/kprobe BPF loading.
3. **Dual Fix Applied (Kernel + Userspace):**
   - **Kernel `#62` (`8f55c2473`):** Enabled `CONFIG_FTRACE`, `CONFIG_TRACING`, `CONFIG_EVENT_TRACING`, `CONFIG_FUNCTION_TRACER`, `CONFIG_DYNAMIC_FTRACE`, `CONFIG_KPROBES`, `CONFIG_KPROBE_EVENTS`, `CONFIG_UPROBE_EVENTS`, `CONFIG_BPF_EVENTS`, `CONFIG_DEBUG_INFO_DWARF5`, `CONFIG_DEBUG_INFO_BTF=y`, `CONFIG_DEBUG_INFO_BTF_MODULES=y`. Verified `[17] .BTF PROGBITS` and `[18] .BTF_ids PROGBITS` in `vmlinux`.
   - **Userspace (`patches/system_memory_libmeminfo/`):** Guarded `ReadPerProcessGpuMem()` and `ReadProcessGpuUsageKb()` in `system/memory/libmeminfo/sysmeminfo.cpp` with `if (access(kBpfGpuMemTotalMap, F_OK)) return false;` so missing `gpu_mem_total_map` gracefully skips instead of aborting `system_server`.

---

## 2. Audio Policy & `StreamOutOffloadStub` Discovery

- In Boot `#0`, `/vendor/etc/primary_audio_policy_configuration.xml` was still the generic Cuttlefish layout (`Speaker` + `FM Tuner` + `compressed_offload`), missing `Earpiece`, `Wired Headset`, `Wired Headphones`, and `Built-In Back Mic`.
- Inspecting `hardware/interfaces/audio/aidl/default/primary/` (`StreamPrimary.cpp` & `ModulePrimary.cpp`) revealed two critical behaviors:
  1. `StreamPrimary::getCardAndDeviceId()` parses `CARD_<c>_DEV_<d>` from `devicePort` `address` attributes, and `useStubStream()` returns `false` for `Earpiece`, `Speaker`, `Wired Headset`, `Wired Headphones`, and mic inputs.
  2. `ModulePrimary::createOutputStream()` intercepts `AudioOutputFlags::COMPRESS_OFFLOAD | NON_BLOCKING` and constructs **`StreamOutOffloadStub`**, which only **simulates playback timing silently** without writing to ALSA!
- **Fix Applied (`device/oneplus/dumpling/configs/audio/primary_audio_policy_configuration.xml` & `init.oneplus5t.hw.sh`):**
  - Mapped `Speaker` (`CARD_0_DEV_0` / `MultiMedia1`), `Built-In Mic` / `Back Mic` / `Wired Headset Mic` (`CARD_0_DEV_1` / `MultiMedia2`), `Wired Headset` / `Wired Headphones` (`CARD_0_DEV_2` / `MultiMedia3`), and `Earpiece` (`CARD_0_DEV_3` / `MultiMedia4`).
  - **Removed `compressed_offload`** so AudioFlinger decodes compressed formats in software and plays real PCM via `primary output` -> ALSA.
  - Added `SLIMBUS_1_RX Audio Mixer MultiMedia4` (`EAR PA`), `SLIMBUS_0_RX Audio Mixer MultiMedia3` (`HPHL/R`), `QUAT_MI2S_RX Audio Mixer MultiMedia1` (`Speaker Switch`), and `MultiMedia2 Mixer SLIMBUS_0_TX` (`ADC2/ADC3`) to `init.oneplus5t.hw.sh` (and fixed a stray syntax fragment in that script, verified `sh -n`).

---

## 3. Multicolor Notification LED (`rgb:status`)

- Boot `#0` `hardware/lineage/interfaces/light` only probed `"charging"`, `"left"`, `"white"`, `"white:indicator"`, `"white:status"` (not `"rgb:status"`), and only wrote `brightness`.
- **Fix Applied (`patches/hardware_lineage_interfaces/` + `init.qcom.rc`):** Added `"rgb:status"` to `kNotificationLedDevices`, detected `mSupportsMultiIntensity` (`multi_intensity` sysfs node), wrote `"R G B"` before `setBrightness()`, and set `chmod 0664` on `/sys/class/leds/rgb:status/{brightness,multi_intensity}`. Compiled cleanly via `m android.hardware.light-service.lineage`.

---

## 4. Early GPU Firmware, SELinux Namespace & Remaining Logs

- **Early GPU Firmware:** `dmesg_boot0.txt` (`~1.4s`) showed `-2` (`-ENOENT`) before `/vendor` mounted; copied `a530_pm4.fw`, `a530_pfp.fw`, and `a540_gpmu.fw2` into `$(TARGET_COPY_OUT_RAMDISK)/vendor/firmware/qcom/` in `device.mk`.
- **SELinux Vendor Property Namespace:** Android 17 `check_prop_prefix` enforces `vendor_` or `odm_` prefix on vendor `property_contexts` types. Defined `vendor_internal_prop(vendor_wifi_hal_prop)` in `sepolicy/vendor/property.te`, updated `property_contexts` (`vendor.wlan.firmware.version`) and `hal_wifi_default.te`, labeled `/dev/rfkill` as `rfkill_device`, replaced `rmtfs block_device:blk_file r_file_perms` (`neverallow`) with `getattr`, and added `hal_graphics_composer_default sysfs:dir r_dir_perms`. Verified **`m selinux_policy`** passed cleanly.
- **Unresolved Minor Warnings Observed in `logcat_boot0.txt` (To Address Next):**
  - `VibratorInfo`: `android.hardware.vibrator-service.oneplus5t` (`vibrator/Vibrator.cpp`) returns `150.0f` for `getResonantFrequency()` while returning `EX_UNSUPPORTED_OPERATION` for `getFrequencyToOutputAccelerationMap()`, causing `VibratorInfo` warning spam.
  - `hal_wifi_default`: WCN3990 `ath10k` returns `code 4` (`EX_UNSUPPORTED`) for optional TWT / multi-STA / radio-combination queries cleanly without crashing Wi-Fi.
  - `external/libcamera`: Bundled LineageOS 24 `external/libcamera` lacks `src/libcamera/software_isp/` (needs upstream `libcamera` SoftISP to bring up `camss` UI camera).
  - Non-shell AVCs still remaining if switching from `permissive` to `enforcing`: `audioserver tmpfs:file`, `system_suspend sysfs:file`, `system_server` `ksm/pages_shared`, `tqftpserv`/`rmtfs`/`diag_router` `qipcrtr_socket:read`.
