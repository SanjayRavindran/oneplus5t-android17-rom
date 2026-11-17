# OnePlus 5T (`dumpling` / MSM8998): Stock Kernel `4.4.205` vs Mainline `6.18` Engineering Knowledge

This document records the concrete hardware, bootloader, Device Tree, clock, regulator, IOMMU, TrustZone, and subsystem discoveries obtained from analyzing **OnePlusOSS `android_kernel_oneplus_msm8998` (`4.4.205`, OOS 10.0.0, commit `2103804389`)**, `stock_analysis` (`stock_defconfig`, `stock_02.dts`, `kallsyms.txt`), decompiled boot images (`abl.img`, `hyp.img`, `tz.img`), and bringing up **Mainline Linux `6.18.0-msm8998+` (`#1`–`#62`)**.

---

## 1. Bootloader (`abl.img` / `LinuxLoader.efi`) & Memory Map

Decompilation of stock `abl.img` (`rea_outputs/abl_decomp`) established strict constraints that mainline builds must obey:

| Parameter | Value / Rule | Verification Source |
|---|---|---|
| **MSM ID** | `qcom,msm-id = <292 0x20001>` (`MSM8998 v2.1`) | `DeviceTreeAppended()` (`0xe7f8.c`) |
| **Board ID** | `qcom,board-id = <8 0 17801 43>` (`MTP 8`, `project 17801`, `hw_version 43`) | `DeviceTreeAppended()` (`0xe7f8.c`) |
| **Kernel Load Ceiling** | `KernelLoadAddr = 0x80080000`, `EndOfKernelRegion = 0x85600000` (`86 MiB` max for decompressed `Image` + `0x201000` DTB + `initramfs` + gzip scratch) | `BootLinux()` (`0xb2c0.c`) |
| **Verified Boot Mode** | Verified Boot v1.0 (**not** AVB 2.0 `avb_custom_key`). Locked `RED` on both `boot` and `recovery` traps into EDL (`05c6:9008`). | `abl.img` (`0x2dc94.c`, `0x1dd28.c`) |
| **Unattended Reboot** | IMEM `imem@146bf000` offset `0x65c`: `mode-bootloader = <0x77665500>`, `mode-recovery = <0x77665502>` (`CONFIG_SYSCON_REBOOT_MODE=y` + `CONFIG_POWER_RESET_QCOM_PON=y`). | Verified live via `reboot bootloader` |

### `reserved-memory` Carveouts Matched to Stock (`stock_02.dts` -> `msm8998-oneplus-common.dtsi`)
Upstream `msm8998.dtsi` defaults do **not** match OnePlus 5T modem/ADSP/SLPI sizes:
- `adsp_mem@8b200000` (`30 MiB`), `mpss_mem@8d000000` (`112 MiB`), `venus_mem@94000000` (`5 MiB`), `mba_mem@94500000` (`2 MiB`), `slpi_mem@94700000` (`15 MiB`), `ipa_fw_mem@95600000` (`64 KiB`), `ipa_gsi_mem@95610000` (`20 KiB`), `gpu_mem@95615000` (`1 MiB`), `wlan_msa_mem@95715000` (`1 MiB`), `cont_splash_mem@9d400000` (`0x02400000`), and CMA `qseecom_region` (`0x86600000` / `0x1400000`).

---

## 2. Clocks, Regulators, TrustZone & Storage

1. **Eliminating `clk_ignore_unused` / `pd_ignore_unused`:** Adding `&gcc protected-clocks` protects critical stock-unmanaged bootloader/RPM clocks so mainline `6.18` boots cleanly without debug cmdline hacks (`d4da34df4`). (One benign `clk_branch2_disable` warning remains on `gcc_rx1_usb2_clkref_clk` before `dwc3` initializes `qusb2phy`.)
2. **`CONFIG_NVMEM_QCOM_QFPROM=y` Built-in:** Required early by `qusb2phy` (`qusb2p_hstx_trim`) so `dwc3` (`a800000.usb`) enumerates USB Gadget (`18d1:4ee7`), and by `CONFIG_QCOM_TSENS=y` (`21` thermal zones).
3. **`CONFIG_HWSPINLOCK_QCOM=y` + `SMEM` + `SMP2P` + `QCOM_PD_MAPPER=y` Built-in:** Required before remoteprocs probe so in-kernel `SERVREG_LOCATOR_SERVICE` (`QMI 64`) serves ADSP/SLPI protection domain maps.
4. **UFS Storage (`1da4000.ufshc`) + Inline Crypto Engine (`1db0000.crypto`):** Upstream `ufshcd` ICE manages FBE inline crypto independently of stock `crypto/ice.h`. Adding `UFS_IOCTL_QUERY` + `CONFIG_CHR_DEV_SG=y` + `CONFIG_SCSI_UFS_BSG=y` restores `/dev/sg0..8` and `/dev/bsg/ufs-bsg0` needed for RPMB (`3deb9bd3d`).
5. **QSEE `/dev/qseecom` (`d7f11f9cd`):** Upstream `drivers/tee/qcomtee/` (`SMCINVOKE`) returns `-EIO` on MSM8998 TZ, and `drivers/firmware/qcom/qcom_qseecom.c` lacks `/dev/qseecom` chardev, `.mdt`/`.bXX` TA dynamic loading, and listener callbacks. Clean-room `/dev/qseecom` on top of `qcom_scm_call()` + CMA `dma_buf` (`qsee_version=0x1001000`, `qseos_version=0x14`) verified `cmnlib`/`cmnlib64`/`gfp5288`/`faceapp`/`soter64` TA loads live.
6. **XPU-Locked GPIOs `0..3` (`blsp1_spi1`):** Removing `<0 4>` from `&tlmm gpio-reserved-ranges` triggers an immediate TrustZone XPU SError crash into fastboot. Consequently, clean-room `nxp_p61_ese` (`b61b2af01`) cannot run on HLOS and `&blsp1_spi1` stays `disabled`; NFC uses `pn553` over I2C (`nfc0`, `3-0028`).

---

## 3. Display Pipeline (`DPU 3.0` + `10nm PHY` + `S6E3FC1`) & Adreno 540 GPU

Comparing stock `mdss_dsi_panel.c` / `msm_kgsl` vs mainline `drm/msm`:

- **Display Contention & Early Boot:** `framebuffer0` (`simple-framebuffer`) must stay disabled in DT so `msm_dpu` owns display scanout exclusively.
- **10nm DSI PLL & MMCC:** Required `dsi_pll_config_8998_regs()` in `dsi_phy_10nm.c`, `CLK_OPS_PARENT_ENABLE` + `mdss_byte{0,1}_intf_div_clk` in `mmcc-msm8998.c`, dropping invalid `ctl_0..4` `.intr_start` in `dpu_3_0_msm8998.h`, and `arm-smmu-qcom` MDSS support (`570e89064`).
- **DPU < 5.0 Command-Mode TE Wait (`8700ef046`):** MSM8998 DPU 3.0 has **no `INTR_IDX_CTL_START` IRQ**. Without waiting on tearing effect (`_dpu_encoder_phys_cmd_wait_for_te()` / `wait_for_idle()` -> `dpu_encoder_wait_for_tx_complete()`), atomic commits race active command-mode transfers.
- **Samsung `S6E3FC1` Panel (`panel-samsung-s6e3fc1.c`):**
  - GPIOs: `poc=GPIO_66`, `vci=GPIO_21`, `reset=GPIO_94`, `LCD_TE=GPIO_10`, `MIPI_ERR_FG_H=GPIO_62`.
  - Brightness writes (`MIPI DCS 0x51`, big-endian 10-bit DBV `__swab16`) must clear `MIPI_DSI_MODE_LPM` during transfer (HS mode) and restore LPM afterward (`8700ef046`).
  - Default/post-resume brightness capped at **`51/1023` (5%)** to prevent AMOLED burn-in during bring-up; exposed `srgb`, `dci_p3`, and `hbm` sysfs nodes using stock DCS tables (`0xf0 0x5a 0x5a`, `0xbc ...`).
- **Adreno 540 TZ CB0 TTBR1 Restriction (`edefa6e09`):**
  - MSM8998 TrustZone/Hypervisor blocks `CP_SMMU_TABLE_UPDATE` on SMMU CB0.
  - Fix: Keep `qcom_adreno_can_do_ttbr1()` returning `false` for `qcom,msm8998-smmu-v2` in `arm-smmu-qcom.c`, set `a5xx_gpu` `.create_private_vm = NULL` in `a5xx_gpu.c` so all GPU clients share the global TTBR0 `gpu->vm`, and skip closing `gpu->vm` on per-client `msm_submitqueue_close()` (`!to_msm_vm(ctx->vm)->pid`) and per-fd `msm_gem_close()` (`ctx->vm == priv->gpu->vm`). Otherwise concurrent SurfaceFlinger/app processes tear down active page tables and trigger IOMMU translation faults / black frames!
  - Set `&pm8005_s1` `regulator-min-microvolt = <1088000>` and restored all Adreno 540 OPPs (`342–710 MHz`), with `msm_gpu_devfreq.c` `polling_ms = 20`.

---

## 4. Video Codec (`Venus 4.2`), Audio (`WCD9335` + `TFA9890`), Cameras (`CAMSS`), Power & Input

- **Venus (`venus_core` / `venus_dec` / `venus_enc`, `f66691c0f`):**
  - Upstream probe failed on 8 GiB RAM until `msm8998` Venus memory/clock fixes (`vcodec_power_v3` `hwmode` sequencing, `mnoc_maxi`/`smmu`/subcore clocks, wrapper resets before ARM9 release).
  - Fixed `IS_V3` synthetic zero-buffer EOS `FBD`/`ETB`, `parse_raw_formats()` heterogeneous `num_planes` HEVC cap overrun, `HFI_PROPERTY_PARAM_VENC_H264_TRANSFORM_8X8`, and runtime PM balance across `H264`/`HEVC`/`VP8`/`MPEG4`/`H263` encode (`/dev/video7`) + decode (`/dev/video6`).
- **Audio (`0 [O5T]`, `WCD9335` v2.0 + `wcd9335-cpe` + `TFA9890`):**
  - Stock DTS/codec clock mapping showed `WCD9335` requires **`divclk1`** (`PM8998 GPIO 13`, `RPM_SMD_DIV_CLK1`, `reset-gpios = tlmm 64`), **not** `wcd934x`.
  - Fixed `qcom-ngd-ctrl` QMI completion race across SSR/PDR (`ef2094428`).
  - Added clean-room `wcd9335-cpe` Xtensa firmware loader (`24ea686cf`..`503f6bb7d`) for `cpe_9335.mdt` (`running=1`, `cmi_ver=1`, `MEM_BANK_0`, SIDO regmap, Tasha v2 RCO/FLL).
  - `TFA9890` (`3-0036`, rev `0x80`) runs in CoolFlux DSP-bypass mode (`tfa989x.c`, no `tfa98xx.cnt` EQ).
  - **Crucial `q6routing` Constraint:** A single `MultiMedia<N>` DAI links to one backend at a time; setting both `QUAT_MI2S_RX` (Speaker) and `SLIMBUS_0_RX` (Headphones) on `MultiMedia1` silently breaks Speaker. Independent FE bindings must be used: `MM1` → Speaker (`QUAT_MI2S_RX`), `MM2` ← Mics (`SLIMBUS_0_TX` `ADC2/3`), `MM3` → Headphones (`SLIMBUS_0_RX`), `MM4` → Earpiece (`SLIMBUS_1_RX` `EAR PA`).
- **Triple Cameras (`ca00020.camss` + `ca0c000.cci`):**
  - `gcc-msm8998.c` `gpll0_out_main_div2`, `mmcc` `mclk0..3_clk_src` `.mnd_width = 8`, `camss_top_gdsc.pd.parent = &bimc_smmu_gdsc.pd`.
  - `i2c-qcom-cci.c` holds `vreg_lvs1a_1p8` (`vio`) across active PM and implements `cci_halt()` IRQ reset recovery.
  - Clean-room `sony-imx-op5t.c` + `lc898217xc-op5t` VCM + `gt24c64` EEPROM stream RAW10 (`SRGGB10P`) via `IMX398` (`0-001a`), `IMX371` (`0-0010`), and `IMX376K` (`1-0010`). Stock `mm-qcamera-daemon`/`libchromatix_*.so` cannot run without downstream `msm_isp` ioctls.
- **Dash Charge (`oneplus_dash.c`, `9a094c20e`):**
  - Schematic (`U4301` `STM8S003F3U6TR`, `TXS0104` `U4303`, `MAX14743` `U4302`, `MCU_EN=GPIO 96`, `4-0026`) + `dashd` REA revealed an `~1850 ms` I2C bootloader window (`100 kHz`, `ap_clk=0`) after `mcu_en` `1 -> 0` reset while keeping `usb_switch=0` (`GPIO 90/91=0`). Verifies `oneplus/dash_stm8s.bin` (`5746` bytes, `ver 0x37`).
- **USB (`dwc3` `a800000.usb` + `qcom_smbx.c`):** USB 2.0 Type-C DRD/OTG works via PMI8998 `typec/port0`; USB 3.0 SuperSpeed lanes are **physically absent** on the OnePlus 5T Type-C receptacle (`maximum-speed = "high-speed"`).
- **Touchscreen (`Synaptics s3508`), Fingerprint (`goodix_op5t`), QRTR:**
  - `s3508` uses level-low IRQ and `syna,wakeup-doze` (`KEY_WAKEUP` DT2W) + wake supplies for `s2idle`.
  - `goodix_op5t` + `oneplus,fpdetect` (`ae3159abd`) drives `reset=tlmm 75`, `enable=tlmm 65`, `irq=tlmm 121`, Netlink group `25`, matching stock `gf_spi.c` `USE_PLATFORM_BUS=1`.
  - `net/qrtr/ns.c` (`51bfc0258`) skips congested local server sockets during `BYE`/`DEL_CLIENT` broadcast.

---

## 5. Android 17 Userspace Kernel Config Lessons (`#62`, `8f55c2473`)

Real Android 17 (`LineageOS 24`) boot diagnostics (`dmesg_boot0.txt`) showed that missing tracing/BTF options break `BpfLoader-rs`:
- **Failed on `#61` (`CONFIG_DEBUG_INFO_NONE=y`, `# CONFIG_FTRACE`, `# CONFIG_KPROBES`):**
  - `tracefs` failed to mount; `timeInState.bpf` & `dmabufIter.bpf` failed with `os error 3` (`ESRCH`); `gpuMem.bpf`, `gpuWork.bpf`, `bpfMemEvents.bpf`, `kernelWakelockDuration.bpf` failed with `os error 22` (`EINVAL`). Missing `/sys/fs/bpf/map_gpuMem_gpu_mem_total_map` caused `libmeminfo.so` (`ReadProcessGpuUsageKb`) -> `BpfMapRO::abortOnMismatch` -> **`system_server` `SIGABRT` crash loop**!
- **Fixed in `#62` (`8f55c2473`):** Enabled `CONFIG_FTRACE=y`, `CONFIG_TRACING=y`, `CONFIG_EVENT_TRACING=y`, `CONFIG_FUNCTION_TRACER=y`, `CONFIG_DYNAMIC_FTRACE=y`, `CONFIG_KPROBES=y`, `CONFIG_KPROBE_EVENTS=y`, `CONFIG_UPROBE_EVENTS=y`, `CONFIG_BPF_EVENTS=y`, `CONFIG_DEBUG_INFO_DWARF5=y`, `CONFIG_DEBUG_INFO_BTF=y`, `CONFIG_DEBUG_INFO_BTF_MODULES=y` (`readelf -S vmlinux` confirmed `.BTF` and `.BTF_ids` sections).
- **Remaining Non-Fatal Mainline 6.18 Notes:**
  1. `fuseMedia.bpf` fails (`os error 2`) because mainline `6.18` lacks out-of-tree Android `CONFIG_FUSE_BPF` (marked non-critical in `bpfloader.rs`).
  2. `NetBpfLoad` logs `W NetBpfLoad: Android V+ requires 6.18 kernel to be 6.18.9+.` (non-fatal warning; all 5 networking BPF programs still load).
