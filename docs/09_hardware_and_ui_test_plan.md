# OnePlus 5T (`dumpling` / MSM8998) Android 17 Hardware & UI Test Plan and Bring-Up Log

**Target Hardware:** OnePlus 5T (`ONEPLUS A5010`, `dumpling`, serial `5f5440e4`)
**Kernel Baseline:** Mainline Linux `6.18.0-msm8998+` (`#62` `8f55c2473` — verified live on `#58`, `#61` LineageOS 24 Boot #0, `#62` built & staged for next ROM build)
**Userspace Baseline:** LineageOS 24 (`lineage_dumpling-cp2a-userdebug`, API `37.0`, Mini GApps via `vendor/gms`)

> See also [00_START_HERE_FUTURE_PICKUP.md](./00_START_HERE_FUTURE_PICKUP.md) and [11_lineage24_boot0_diagnostics_and_fixes.md](./11_lineage24_boot0_diagnostics_and_fixes.md) for the full Boot `#0` root-cause analysis and next-build verification checklist.

---

## 1. Root Causes Resolved & Hardware/HAL Matrix

| # | Subsystem | Latest Status | Root Cause Resolved | Implementation |
|---|---|---|---|---|
| 1 | **GPU Acceleration (`Adreno 540`) & Display Rendering** | **PASS (UI Verified on `#58` + Kernel `#62`)** | `drm/msm` global `gpu->vm` was torn down on per-client `msm_submitqueue_close()` / `msm_gem_close()`; DPU `< 5.0` lacked `CTL_START` CMD TE sync; S6E3FC1 brightness needed HS mode; early `1.4s` GPU FW hit `-ENOENT`. | Commits `8700ef046` & `edefa6e09` (`_dpu_encoder_phys_cmd_wait_for_te`, `create_private_vm=NULL`, global `gpu->vm` teardown guard, `pm8005_s1 >= 1.088V`, `srgb`/`dci_p3`/`hbm` sysfs); early FW in ramdisk via `device.mk`; Mesa3D `freedreno` + `minigbm` + `drm_hwcomposer`. |
| 2 | **`system_server` eBPF Boot `#0` Crash (`libmeminfo` + `#62`)** | **FIXED IN CODE (Pending Next Build Flash)** | Kernel `#61` lacked `FTRACE`/`KPROBES`/`DEBUG_INFO_BTF`, breaking `BpfLoader-rs` (`gpuMem.bpf` / `timeInState.bpf`) and triggering `BpfMapRO::abortOnMismatch` `SIGABRT` in `ReadProcessGpuUsageKb`. | Kernel `#62` (`8f55c2473`) enables `FTRACE`/`KPROBES`/`BTF` (`.BTF`/`.BTF_ids` verified in `vmlinux`); `sysmeminfo.cpp` also checks `access(kBpfGpuMemTotalMap, F_OK)`. |
| 3 | **USB Gadget (`dwc3`), `s2idle` Lock & MTP/ADB** | **PASS (`#58` & Boot `#0`)** | Complete ConfigFS `ffs.adb`/`ffs.mtp`/`ffs.ptp`/`rndis.gs4` rules in `init.qcom.usb.rc`. | `DumplingStayAwake` wake_lock in `init.qcom.rc`; verified `/sys/class/typec/port0`. *(Note: revisit `DumplingStayAwake` before daily-driver battery use.)* |
| 4 | **Wi-Fi (`WCN3990` `ath10k_snoc` `wlan0`)** | **PASS (Scan & HAL + SELinux Verified)** | Static `libwifi-hal-ath10k` (`device/oneplus/dumpling/wifi_hal/`); `/system/bin/ip` MAC provisioning; `vendor_wifi_hal_prop` SELinux namespace fix (`check_prop_prefix` passed). | Scans 2.4 GHz + 5 GHz SSIDs cleanly (`cmd wifi`). |
| 5 | **Bluetooth (`WCN3990` `hci0`)** | **PASS (`#58`)** | `oneplus5t-bt-init` reads `/mnt/vendor/persist/bluetooth/.bt_nv.bin` and sets public BD_ADDR via `MGMT_OP_SET_PUBLIC_ADDRESS`; `/dev/rfkill` labeled `rfkill_device`. | Auto-enables `ON`. |
| 6 | **Vibrator (`spmi_haptics`)** | **PASS (`#58`)** | Clean-room `android.hardware.vibrator-service.oneplus5t` using `EV_FF` on `/dev/input/event*` (`spmi_haptics`). | Verified `cmd vibrator_manager synced oneshot` & `feedback 1`. *(Minor TODO: fix `VibratorInfo` `resonantFrequencyHz` log warning.)* |
| 7 | **SLPI Sensors (`0x100` `SNS_SMGR_SVC`)** | **PASS (`#58` + Boot `#0`)** | `android.hardware.sensors-service.oneplus5t` over `AF_QIPCRTR`: `BMI160` accel/gyro, `AK09911` mag, `PA224` prox, `TCS3400` ALS + 8 fused sensors. | Added `hal_sensors_default system_server_tmpfs` SELinux rules. |
| 8 | **Audio (`O5T` Card 0: `TFA9890` + `WCD9335`)** | **HW PASS; Policy Fixed for Next Build** | Replaced Cuttlefish XML with OnePlus 5T `primary_audio_policy_configuration.xml` (`MM1` Speaker `CARD_0_DEV_0`, `MM2` Mic `CARD_0_DEV_1`, `MM3` Headphones `CARD_0_DEV_2`, `MM4` Earpiece `CARD_0_DEV_3`), removed `compressed_offload` (`StreamOutOffloadStub` silent bug), and fixed `init.oneplus5t.hw.sh` syntax + mixer paths. | Verified `tinyplay` & capture on `#58` (`asound.txt` Card 0 active on Boot `#0`). Retest Speaker/Earpiece/Headphones/Mic UI playback on next build. |
| 9 | **Lights (`rgb:status` + Backlight), Battery, Thermal & Keys** | **Backlight/Battery PASS; `rgb:status` Fixed** | Patched `hardware/lineage/interfaces/light` for `rgb:status` (`multi_intensity`) + `init.qcom.rc` `0664` perms; compiled cleanly (`android.hardware.light-service.lineage`). | `bq27411-0`, `pmi8998` thermal zones, `s3508`, alert slider, Hall sensor active. |
| 10 | **Deferred Clean-Room Subsystems** | **KERNEL READY / HAL DEFERRED** | **Camera:** `camss` RAW10 V4L2 ready; bundled LineageOS `external/libcamera` lacks `software_isp/`. **Telephony:** `MPSS`/`QMI_UIM`/`rmnet_ipa0` ready; full `IRadio*` AIDL HAL needed. **NFC & Goodix FP:** PN553 `AF_NFC` HAL + QSEE `libion`/AIDL wrappers needed. | Next roadmap items after Boot `#2` passes cleanly. |

---

## 2. Live Verification Evidence (`#58` & LineageOS 24 Boot `#0`)

- **GPU & Display (`#58`):** `dmesg | grep -c "gpu fault:"` = **`0`**; `/sys/class/backlight/c994000.dsi.0/{brightness,max_brightness,bl_power}` writable (`bl_power=0`).
- **Remoteprocs & Services (Boot `#0` `service_list.txt` & `dmesg_boot0.txt`):** `rmtfs`, `tqftpserv`, `sns-reg2-qmi`, `diag-router` online; `slpi`, `adsp`, `mpss` running; `wcd9335-cpe` (`cpe_9335.mdt` `cmi_ver=1`) booted; `NetBpfLoad` loaded all 5 networking BPF programs.
- **LineageOS 24 SELinux Build Check (2026-11-17):** `m -j16 selinux_policy android.hardware.light-service.lineage` **succeeded (`exit 0`)**.
- **Next-Build Flash Checklist (Once Resumed):**
  1. Finish `m -j16 bacon bootimage systemimage vendorimage recoveryimage` on `.15`.
  2. Verify `BpfLoader-rs` loads `timeInState.bpf`, `gpuMem.bpf`, `gpuWork.bpf`, `bpfMemEvents.bpf`, `kernelWakelockDuration.bpf`, and `dmabufIter.bpf` cleanly with zero `system_server` `SIGABRT`.
  3. Verify `sys.boot_completed=1`, audio routing across Speaker/Earpiece/Headphones/Mic, and `rgb:status` notification LED colors.
