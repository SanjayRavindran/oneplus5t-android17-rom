---
name: msm8998-mainline-android-kernel
description: >-
  Use this skill when configuring, building, packaging, or RAM-boot testing the
  mainline Linux 6.18 LTS kernel (qcom-msm8998-6.18.y) for the OnePlus 5T
  (qcom/msm8998-oneplus-dumpling.dts) with Android 17 required kernel features.
---

# MSM8998 Mainline Linux 6.18 LTS + Android 17 Kernel Bring-Up Skill

This skill provides the hardware-verified workflow to build **Linux 6.18 LTS (`qcom-msm8998-6.18.y`)** for the OnePlus 5T (`oneplus-dumpling`) combining upstream `msm8998` hardware support (`msm_dpu`, Adreno 540 `a5xx`, `venus` video codec, `qcom-smbx-charger` PMI8998 SMB2, `bq27411` fuel gauge, ported `panel-samsung-s6e3fc1` & `qcom-pmi8998-haptics`, in-kernel `qcom_pd_mapper`, and `remoteproc` ADSP/SLPI/MPSS) with Android 17 kernel features.

> **Current Kernel Baseline (`#62`, `dumpling-6.18-android17` commit `8f55c2473`):** Clean boot cmdline (`clk_ignore_unused` / `pd_ignore_unused` absent via `&gcc protected-clocks`), `CONFIG_SUSPEND=y` (`s2idle` verified), Display (`DPU 3.0` CMD-mode TE wait `_dpu_encoder_phys_cmd_wait_for_te` + `S6E3FC1` `1080x2160` + `5%` brightness `51/1023` + HS brightness writes + `srgb`/`dci_p3`/`hbm` sysfs), `Adreno 540` (`/dev/dri/renderD128` sharing global TTBR0 `gpu->vm` + `342–710 MHz` OPPs + `pm8005_s1 >= 1.088 V`), `Synaptics s3508` touch (DT2W `KEY_WAKEUP` + wake supplies), UFS + ICE (`1db0000.crypto`), `/dev/qseecom` (`qseos_version=0x14`) + `/dev/sg0..8` RPMB (`CONFIG_CHR_DEV_SG` + `CONFIG_SCSI_UFS_BSG`), `goodix_op5t` + `fpdetect` (`/dev/goodix_fp`, `0x03 (gfp5288)`), `QCE` (`1dfa000.crypto`), `qcom_rng`, Venus `4.2` (`/dev/video6`, `/dev/video7`), `msm8998-osm-cpufreq` + NoC/BWMON, Audio (`WCD9335` + `wcd9335-cpe` `cpe_9335.mdt` `running=1` + `TFA9890`, `0 [O5T]`), USB Type-C/OTG + `oneplus_dash` STM8S (`4-0026`), Triple Camera (`imx398`/`imx376k`/`imx371` + `lc898217xc` VCM), `WCN3990` Wi-Fi (`wlan0`), BT `hci0`, NFC `nfc0`, `bq27411-0`, RGB/flash LEDs, tri-state slider, Hall sensor, SPMI haptics, IPA v3.1 (`rmnet_ipa0`), MPSS QMI, SLPI `SNS_SMGR_SVC`, IMEM `syscon-reboot-mode`, QRTR congestion fix (`51bfc0258`), and **Android 17 `FTRACE`/`KPROBES`/`DEBUG_INFO_BTF`** (`vmlinux` `.BTF` + `.BTF_ids` verified via `readelf -S`).

## 1. Decompiled `abl.img` (`LinuxLoader.efi`) Boot Contract
- **Device Tree Source:** `arch/arm64/boot/dts/qcom/msm8998-oneplus-dumpling.dts` (includes `msm8998-oneplus-common.dtsi` and `msm8998.dtsi`)
- **Board IDs Matched by `DeviceTreeAppended()` (`0xe7f8.c`):**
  - `qcom,msm-id = <292 0x20001>` (`MSM8998 v2.1`)
  - `qcom,board-id = <8 0 17801 43>` (`MTP 8`, `project = 17801`, `hw_version = 43`)
- **Memory Layout Enforced by `BootLinux()` (`0xb2c0.c`):**
  - `KernelLoadAddr = 0x80080000`
  - `EndOfKernelRegion = 0x85600000` (`86 MiB` total ceiling for decompressed `Image` + `0x201000` DTB reserve + `initramfs.cpio.gz` + gzip scratch buffer)

## 2. Critical Hardware & Android 17 Bring-Up Rules
1. **`CONFIG_NVMEM_QCOM_QFPROM=y` (Built-in):** Mandatory for `qusb2phy` (`nvmem-cells = <&qusb2p_hstx_trim>`) so `dwc3` (`a800000.usb`) enumerates USB Gadget (`18d1:4ee7`), and for `CONFIG_QCOM_TSENS=y` (21 thermal zones).
2. **`CONFIG_HWSPINLOCK_QCOM=y`, `CONFIG_QCOM_SMEM=y`, `CONFIG_QCOM_SMP2P=y`, `CONFIG_QCOM_PD_MAPPER=y` (Built-in):** Mandatory so `tcsr_mutex` (`hwlock@1f40000`), `smem`, `smp2p`, and in-kernel `SERVREG_LOCATOR_SERVICE` (`QMI 64`) initialize before remoteprocs probe.
3. **`CONFIG_QCOM_Q6V5_PAS=m` & `CONFIG_QCOM_Q6V5_MSS=m` (Modules):** Load via `modprobe` *after* mounting `/mnt/vendor/persist` and `/vendor/firmware_mnt`, ensuring `/dev/block/bootdevice/by-name/*` symlinks exist, and starting `/vendor/bin/rmtfs -P -r -o /dev/block/platform/soc@0/1da4000.ufshc/by-name -v`, `tqftpserv`, and `diag-router`.
4. **Display & Adreno 540 GPU Quirks (Fixed in `8700ef046` & `edefa6e09`):**
   - **DPU 3.0 CMD Mode TE Sync:** MSM8998 DPU `< 5.0` lacks `INTR_IDX_CTL_START`; `dpu_encoder_phys_cmd_wait_for_commit_done()` waits on TE (`_dpu_encoder_phys_cmd_wait_for_te()`), while `msm_atomic_commit_tail()` calls `wait_for_idle()` (`dpu_encoder_wait_for_tx_complete`) before pushing updates.
   - **S6E3FC1 Panel:** Clear `MIPI_DSI_MODE_LPM` around `mipi_dsi_dcs_set_display_brightness()` (`0x51`), keep initial/post-resume backlight at `51/1023` (`5%`), and expose `srgb`, `dci_p3`, and `hbm` sysfs nodes.
   - **Adreno 540 TZ CB0 TTBR1 Block:** TrustZone blocks `CP_SMMU_TABLE_UPDATE` on CB0, so `qcom_adreno_can_do_ttbr1()` returns `false` and `a5xx_gpu` sets `.create_private_vm = NULL` to share the global `gpu->vm` (TTBR0). Both `msm_submitqueue_close()` (`!to_msm_vm(ctx->vm)->pid`) and `msm_gem_close()` (`ctx->vm == priv->gpu->vm`) must skip tearing down the shared global `gpu->vm`, or concurrent SurfaceFlinger/UI clients hit SMMU translation faults and black frames. `&pm8005_s1` minimum voltage is `1088000` uV to sustain `342–710 MHz` Adreno OPPs.
5. **Android 17 eBPF & Tracing Requirements (Fixed in `#62` / `8f55c2473`):**
   - Android 17 `BpfLoader-rs` fails to mount `tracefs` / register tracepoint & kprobe BPF maps (`timeInState.bpf` `-ESRCH`, `gpuMem.bpf`/`gpuWork.bpf`/`bpfMemEvents.bpf`/`kernelWakelockDuration.bpf` `-EINVAL`, `dmabufIter.bpf` `-ESRCH`) unless `CONFIG_FTRACE=y`, `CONFIG_TRACING=y`, `CONFIG_EVENT_TRACING=y`, `CONFIG_FUNCTION_TRACER=y`, `CONFIG_DYNAMIC_FTRACE=y`, `CONFIG_KPROBES=y`, `CONFIG_KPROBE_EVENTS=y`, `CONFIG_UPROBE_EVENTS=y`, `CONFIG_BPF_EVENTS=y`, `CONFIG_DEBUG_INFO_DWARF5=y`, and `CONFIG_DEBUG_INFO_BTF=y` are enabled (verify `readelf -S vmlinux` contains `.BTF` and `.BTF_ids`).
   - Note: Mainline `6.18` lacks Android's out-of-tree `CONFIG_FUSE_BPF` (`fuseMedia.bpf` fails non-fatally with `ENOENT`), and `NetBpfLoad` logs a non-fatal warning expecting `6.18.9+`. Userspace `libmeminfo` also needs a guard in `sysmeminfo.cpp` in case `gpu_mem_total_map` is ever absent.
6. **Unattended Fastboot Reboot:** IMEM `syscon-reboot-mode` (`imem@146bf000`, offset `0x65c`, `mode-bootloader = <0x77665500>`, `mode-recovery = <0x77665502>`) enables `reboot bootloader` / `reboot recovery`.

## 3. Build & Package Workflow (`.15`)

### Step 1: Configure Linux 6.18 LTS (`dumpling-6.18-android17`)
```bash
cd ~/oneplus5t-android17/kernel/linux-mainline-6.18
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- oneplus5t_android17_defconfig
```

### Step 2: Compile Kernel, DTB & Modules
```bash
make -j"$(nproc)" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KCFLAGS="-Wno-error" Image.gz qcom/msm8998-oneplus-dumpling.dtb modules
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=/tmp/op5t_mods INSTALL_MOD_STRIP=1 modules_install
cat arch/arm64/boot/Image.gz arch/arm64/boot/dts/qcom/msm8998-oneplus-dumpling.dtb > Image.gz-dtb
```
Verify `.BTF` in `vmlinux`:
```bash
aarch64-linux-gnu-readelf -S vmlinux | grep -E "BTF|btf"
```
Update `device/oneplus/dumpling/prebuilt/kernel/Image.gz-dtb` and the 12 stripped `.ko` modules in `device/oneplus/dumpling/prebuilt/kernel/modules/` before rebuilding Android `boot.img` / `vendor.img`.

### Step 3: Package Standalone RAM-Boot Image (`mkbootimg`, optional)
```bash
mkbootimg \
  --kernel Image.gz-dtb \
  --ramdisk initramfs-android17-6.18.cpio.gz \
  --base 0x00000000 \
  --kernel_offset 0x00008000 \
  --ramdisk_offset 0x01000000 \
  --tags_offset 0x00000100 \
  --pagesize 4096 \
  --cmdline "console=tty0 androidboot.hardware=qcom androidboot.console=ttyMSM0 earlycon" \
  -o boot-6.18.img
fastboot boot boot-6.18.img
```

## References
- Android 17 + MSM8998 Kernel Config Fragment Reference: [android17_kernel_config.md](./references/android17_kernel_config.md)
- Kernel Docs (Stock `4.4.205` vs Mainline `6.18` Architecture): `docs/01_stock_4_4_vs_mainline_6_18_findings.md` (in kernel repo)
