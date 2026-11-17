# Hardware Inventory, Kernel `#52` Status & ROM Readiness

Verified engineering reference for the **OnePlus 5T (`dumpling` / `17801`, `hw_version=43`, `rf_version=21`)** running clean-room mainline **Linux 6.18.0-msm8998+ (`#52`, commit `0ebcc3571`)**.

---

## 1. ABL Bootloader Contract & Build Constraints

From clean-room analysis of `abl.img` (`LinuxLoader.efi`, `229,376` bytes):
- **`KernelLoadAddr`:** `0x80080000`
- **`EndOfKernelRegion`:** `0x85600000` (`86 MiB` hard region ceiling — keep `Image.gz-dtb` + boot layout compact)
- **DTB ID Matchers (`qcom,msm8998`):**
  - `qcom,msm-id = <292 0x20001>`
  - `qcom,board-id = <8 0 17801 43>`
- **Boot Image Format:** Page size `4096`, appended compressed kernel + DTB (`Image.gz-dtb`, `13,157,741` B) + initramfs (`19,566,627` B) -> `boot-6.18-final.img` (`32,735,232` B, SHA-256 `33897082d8acfe599516f24734880a2b576044f66e564428d73def27f9b6df61`).

---

## 2. Kernel `6.18` (`dumpling-6.18-android17`) Commit Stack

Base: `bd4d70bde` (`Linux 6.18.0-msm8998`, upstream `4e0c53280d9757b3872d3823b9f94066669aa388`), defconfig `arch/arm64/configs/oneplus5t_android17_defconfig`:

| Commit | Change |
| :--- | :--- |
| `570e89064` | `#1–#13` baseline: `DPU 3.0` + `S6E3FC1` panel, `Adreno 540`, `S3508` touch, SPMI haptics, DT, defconfig |
| `a38e66a35` | `soc: qcom: pd-mapper`: ADSP and SLPI protection domains for `msm8998` |
| `7e04a3825` | `pm8998`/`oneplus` DT: fix `vref_1p25`, add ADC & ADC-TM thermistors |
| `1b22e2621` | `oneplus` DT: add RGB/flash LEDs, tri-state alert slider, fix NFC GPIOs |
| `ccbd27dbb` | Defconfig: strict DMA IOMMU, flash LED, ADC-TM, DMA-BUF heaps |
| `993b59e2f` | `soc: qcom`: SMP2P AP sleepstate driver + wire SLPI sleepstate (`bit 12`) |
| `7d6e68fda` | `oneplus` DT: drop unpopulated UFS thermistor |
| `47b8420ac`..`444ba0bf3` | `media: venus` & `clk/media: msm8998`: Venus `4.2` VA bounds, subcore clocks/GDSCs (`HW_CTRL_TRIGGER`), wrapper config |
| `7e9df1b1c` | `msm8998` DT & config: PRNG EE node + `qcom-rng` module |
| `80e83c553` | `msm8998` DT & config: IPA v3.1 node, SMP2P states, `ipa` module |
| `690efc4d4` | `drm/msm/gpu`: make `vdd`/`vddcx` regulators optional |
| `6384153ef` | `wifi: ath10k`: make `vdd-3.3-ch1` regulator optional for 1x1 `WCN3990` |
| `00e0e565b` | `msm8998` DT: QCE (`1dfa000.crypto`), `cryptobam`, PM8998 coincell |
| `1b43cf4fe` | `soc: qcom: ufs`: UFS Inline Crypto Engine (`1db0000.crypto`), clock source, built-in crypto drivers |
| `ca36ffc58` | `cpufreq: msm8998`: clean-room OSM v2, CPRh, and SAW4 AVS driver |
| `b32f3b0d3` | `interconnect: qcom`: clean-room MSM8998 NoC + BWMON support |
| `c7f33bcb6` | `msm8998`: add Batch B Type-C/OTG role switch (`qcom_smbx.c`) and QDSP6/ASoC audio (`sound/soc/qcom/msm8998.c`) |
| `e2bcb216a` | `arm64: configs`: enable Batch B Type-C/OTG and QDSP6/ASoC audio for `oneplus5t` |
| `8128921fa` | `arm64: dts: qcom: oneplus5t`: fix audio codec (`WCD9335`/`TFA9890`) and MCLK (`divclk1` on `PM8998 GPIO 13`) |
| `4ac291598` | `msm8998: media`: Batch C triple-camera bring-up (`CAMSS` CSIPHY v5.0.1/ISPIF/VFE 4.8 + `genpd_link` refcounting, `CCI` `vio` runtime hold & halt recovery, `MMCC`/`GCC` MCLK & `camss_top_gdsc`, `sony-imx-op5t`) |
| `37b946a76` | `oneplus5t`: enable Android 17 crypto/verity/bpf/bsg/usb configs and 5% initial AMOLED panel brightness (`51/1023`) |
| `eb7a51cc2` | `oneplus5t`: add clean-room `lc898217xc-op5t` VCM actuators and `24c64` camera EEPROMs |
| `ef2094428` | `slimbus: qcom-ngd-ctrl`: fix QMI completion race across SSR/PDR notifications |
| `d4da34df4` | `oneplus5t`: add `&gcc protected-clocks` (boot without `clk_ignore_unused`/`pd_ignore_unused`) and clean-room `Synaptics S3508` DT2W (`KEY_WAKEUP`, level-low IRQ, wake supplies, `syna,wakeup-doze`) |
| `ef10235b0` | `oneplus5t`: enable `CONFIG_SUSPEND`, `CONFIG_PM_WAKELOCKS`, `CONFIG_PM_AUTOSLEEP` (`s2idle` verified) |
| `3deb9bd3d` | `ufs`: add `UFS_IOCTL_QUERY` and enable `CONFIG_CHR_DEV_SG` (`/dev/sg0..8` for RPMB) |
| `d7f11f9cd` | `misc: qseecom`: clean-room `/dev/qseecom` driver, `qcom_scm` bridge, and `qseecom@86600000` DT region |
| `ae3159abd` | `input: misc`: clean-room Goodix `GF5288`/`fpdetect` driver (`/dev/goodix_fp`, `0x03 gfp5288`) |
| `24ea686cf`..`503f6bb7d` | `ASoC: wcd9335-cpe`: clean-room WCD9335 CPE Xtensa loader (`cpe_9335.mdt`), Tasha v2 RCO/FLL/9.6MHz MCLK, SIDO regmap, `MEM_BANK_0` transfer & idempotent `cpe_load` |
| `5d415c2db` | `arm64: dts`: wire IMEM `syscon-reboot-mode` (`0x65c`) + `CONFIG_SYSCON_REBOOT_MODE=y` for unattended `reboot bootloader`/`recovery` |
| `0ebcc3571` | `docs`: add repository `README.md` with feature summary, disclaimer, and AI disclosure |

---

## 3. Verified Subsystem Inventory (`#52` Live Hardware)

### Working on Hardware Now
- **CPUfreq (`msm8998-osm-cpufreq`):** `policy0` (`300.0–1900.8 MHz`, `22` OPPs), `policy4` (`300.0–2457.6 MHz`, `31` OPPs), CPRh (`cpr_rev=3`) + SAW4 AVS.
- **NoC / BWMON Interconnect:** `mas_gnoc_bimc`, `mas_oxili`, `mas_mnoc_bimc`, `mas_snoc_bimc`, `slv_ebi`, `slv_hmss_l3`, `slv_bimc_snoc_0`, `slv_bimc_snoc_1` active in `interconnect_summary`.
- **Display & GPU:** `msm_dpu` (`c901000.display-controller`) bound `5000000.gpu` (`Adreno 540` with `a530_pm4.fw`, `a530_pfp.fw`, `a540_gpmu.fw2`); `/dev/dri/{card0,renderD128}`; `card0-DSI-1` `connected` (`1080x2160`), backlight `c994000.dsi.0` (`51/1023`, 5% anti-burn-in default & post-resume).
- **Storage & Hardware Crypto:** UFS (`1da4000.ufshc`, `6` `sd*` LUNs) + QC ICE v3.0.65 (`1db0000.crypto`, raw keys `64`/`32` supported, devfreq `simple_ondemand`), `qce` (`1dfa000.crypto`), `qcom_rng`.
- **Video Codecs (Track 4 Verified):** `venus_core`, `venus_dec`, `venus_enc` -> `/dev/video6` (decoder) & `/dev/video7` (encoder); full `H264`, `HEVC`, `VP8`, `MPEG4`, `H263`, and `1080p` `H264`/`HEVC` encode+decode with `last=1` EOS and runtime PM suspend/resume verified live (`f66691c0f`).
- **Touch, Keys, Sensors & Feedback:**
  - `Synaptics s3508` (`0-0020`), `pm8941_pwrkey`, `Volume buttons`, `Tri-state key`, `Hall effect sensor`, `spmi_haptics`
  - `rgb:status`, `white:flash`, `yellow:flash` LEDs
  - `bq27411-0` (`2-0055`) fuel gauge + `pmi8998-charger`, `26` thermal zones
- **Remoteprocs, Connectivity & QMI/QRTR:**
  - `remoteproc0` (`slpi`), `remoteproc1` (`adsp`), `remoteproc2` (`4080000.remoteproc` MPSS) all `running` (`117` QRTR lookup lines)
  - Wi-Fi: `ath10k_snoc` (`WCN3990` hw1.0 `WLAN.HL.1.0.c6-00126`, `wlan0` with provisioned NV MAC via QMI)
  - Bluetooth: `/sys/class/bluetooth/hci0`
  - NFC: `/sys/class/nfc/nfc0` (`1-0028` NXP PN553)
  - Modem & Data: `qcom_q6v5_mss` (`MPSS.AT.2.0.c4.7-00070`) + `ipa` (`1e40000.ipa` -> `rmnet_ipa0`) + `rmtfs` + `tqftpserv` + `pd-mapper` + `diag-router`
  - GNSS / Location: QMI `Location service (~ PDS v2)` (`0x10` v2, port `88`) online on MPSS
  - SLPI Sensors (`0x100` `SNS_SMGR_SVC` + `SNS_REG2_SVC` + SAM services): `BMI160` accel (`10`) & gyro (`20`), `AK09915` mag (`30`), `TMD3725` prox/RGB-ALS (`40`)
- **Audio & USB Type-C / OTG (Batch B):**
  - `adsp` (`adsp.mbn` -> `audio_pd`) + `qcom,apr` (`aprsvc:service:4:{3,4,7,8}`) + `171c0000.slim-ngd` -> **`WCD9335` (`217:1a0:1:0` v2.0)** clocked by **`divclk1` (`PM8998 GPIO 13`)** + **`TFA9890` (`5-0036`/`3-0036`, rev `0x80`)** + `msm8998-snd` -> `0 [O5T]: msm8998-snd - OnePlus 5T` (`MultiMedia1..6`, `/dev/snd/{controlC0,pcmC0D0..5{c,p}}`, `0` deferred devices).
  - PMI8998 Type-C (`typec/port0`, `[dual] source sink`, `host [device]`, `source [sink]`) + `a800000.usb-role-switch` (`dwc3` DRD); `oneplus_dash` (`4-0026`, `STM8S003F3U6TR`) verifies `oneplus/dash_stm8s.bin` (`ver 0x37`, `5746` bytes) while keeping `usb_switch=0` (`gpio90, 91=0`).
- **Cameras (Batch C — Verified Live on `#33`):**
  - `ca0c000.cci` (`i2c-qcom-cci`, `vio-supply = <&vreg_lvs1a_1p8>` runtime hold + `cci_halt()`/IRQ-clear recovery) + `ca00020.camss` (`qcom,msm8998-camss`: CSIPHY v5.0.1 10nm 2PH, CSID v5.0 / 4-7, ISPIF v3.0, VFE 4.8 RDI0..2, `/dev/media0`, `/dev/video0..7`, `/dev/v4l-subdev0..19`).
  - Automatic boot ID + live V4L2 `SRGGB10P` RAW10 streaming verified across all 3 sensors (`0` timeouts/Oops/deferred devices) plus back-to-back `0-001a <-> 0-0010` unbind/rebind stress testing:
    - `imx398` (`0-001a`, Rear Primary): `4656x3496` via `msm_csiphy0 -> msm_csid0 -> msm_ispif0 -> msm_vfe0_rdi0 -> /dev/video0` (`20,360,704` B/frame).
    - `imx371` (`0-0010`, Front): `4656x3456` via `msm_csiphy2 -> msm_csid2 -> msm_ispif2 -> msm_vfe0_rdi1 -> /dev/video1` (`20,127,744` B/frame).
    - `imx376k` (`1-0010`, Rear Tele): `2592x1940` via `msm_csiphy1 -> msm_csid1 -> msm_ispif1 -> msm_vfe1_rdi0 -> /dev/video3` (`6,285,600` B/frame).

---

## 4. AOSP 17 Development Readiness & Remaining Kernel Work

### Ready to Start (`Option B`)
- Display/DRM (`card0-DSI-1`, `renderD128`), touch (`Synaptics s3508`), storage (UFS + ICE raw keys), CPU/bus scaling (`OSM`/`BWMON`), audio (`WCD9335`/`TFA9890`), USB Type-C/OTG, triple-camera V4L2 RAW10 capture, Wi-Fi/BT/NFC, modem QRTR/IPA, SLPI Sensor1, and Venus codecs are working on `6.18`.

### Not Done Yet in Userspace / AOSP (`Option B`)
1. **AOSP Tree & Host Tooling:** No AOSP 17 checkout or `device/oneplus/dumpling/` tree exists yet. On `192.168.1.15`, LVM storage has been expanded (`606 GiB` free on `741G` root LV, `Ubuntu 26.04 LTS`), while host tools `repo`, `adb`, `fastboot`, and `java` still need to be installed before syncing AOSP 17 + TWRP.
2. **Graphics & Camera Userspace:** `Mesa3D` (`freedreno`/`turnip` + GBM gralloc), `drm_hwcomposer`, and `libcamera` SoftISP + AOSP Camera AIDL HAL must be integrated and tested on AOSP 17.
3. **Binder AIDL HALs:** `hals/radio/`, `hals/sensors/`, and `hals/sysfs_hals/` currently compile static diagnostic probes writing `/run/*.json`; real `libbinder_ndk` Android 17 AIDL HAL daemons (`IRadio*`, `ISensors`, `IHealth`, `IPower`, `ILights`, `IVibrator`, `IThermal`, Wi-Fi/BT/GNSS/Audio/USB) still need to be written/integrated.
4. **Security / QSEE Recovery & End-of-Dev Relock:** Initial AOSP 17 bring-up runs unlocked with Software KeyMint + SW Gatekeeper + Widevine L3; optional `/dev/qseecom` TA recovery (Goodix `GF5216` fingerprint, HW Keymaster/Gatekeeper) and final `YELLOW` VB 1.0 bootloader relocking wait until core ROM bring-up is stable (see [`docs/02_signed_firmware_and_security.md`](file:///home/zero/.gemini/antigravity/scratch/oneplus5t/docs/02_signed_firmware_and_security.md)).

### Kernel Finalization Status (`#52`, `.config` / DT / `dmesg`)
Verified directly against `oneplus5t_android17_defconfig`, `msm8998-oneplus-*.dts*`, and live `#51`/`#52` tests (`0` failed probes, `0` kernel Oops/BUG):
1. **Android 17 `.config`, Security & Power Management (Completed in `37b946a76`–`5d415c2db`):** `CONFIG_FS_ENCRYPTION`, `CONFIG_FS_ENCRYPTION_INLINE_CRYPT`, `CONFIG_DM_DEFAULT_KEY`, `CONFIG_DM_VERITY`, `CONFIG_DM_VERITY_FEC`, `CONFIG_FS_VERITY`, `CONFIG_QCOM_QSEECOM_DEV`, `CONFIG_CHR_DEV_SG`, `CONFIG_SCSI_UFS_BSG`, `CONFIG_INPUT_GOODIX_OP5T`, `CONFIG_SND_SOC_WCD9335_CPE`, `CONFIG_SYSCON_REBOOT_MODE`, `CONFIG_SUSPEND`, `CONFIG_PM_WAKELOCKS`, `CONFIG_PM_AUTOSLEEP`, and `5%` AMOLED initial brightness (`51/1023`) are enabled and verified live.
2. **Clean Cmdline & Clocks (Completed in `d4da34df4`):** Boots with clean cmdline (NO `clk_ignore_unused` or `pd_ignore_unused`) via `&gcc protected-clocks`. One benign upstream `clk_branch2_disable` warning remains on `gcc_rx1_usb2_clkref_clk` at `1.64s` before `dwc3` initializes `qusb2phy`.
3. **Tracks 1–3 Ported & Verified Live (`#51` / `#52`):**
   - `/dev/qseecom` (`qseos_version=0x14`), `/dev/sg0..8`, `QCE` AES/SHA-256 passes.
   - `/dev/goodix_fp` (`0x03 (gfp5288)`, Netlink `25`, ioctls verified `#51`).
   - `WCD9335 CPE` (`cpe_9335.mdt` `running=1`, `cmi_ver=1`, `errs=0`, `0->1` reload verified) + `0 [O5T]` ADSP `SLIMBUS_0_RX Audio Mixer MultiMedia1` 9600-frame PCM playback (`tfa989x` remains DSP-bypass).
   - **Track 4 (`venus` Codec `f66691c0f`):** Full `H264`, `HEVC`, `VP8`, `MPEG4`, `H263`, and `1080p` `H264`/`HEVC` encode (`/dev/video7`) + decode (`/dev/video6`) with `IS_V3` zero-buffer EOS, `parse_raw_formats()` HEVC cap fix, `H264_TRANSFORM_8X8`, and runtime PM verified live; `lc898217xc` VCM `/dev/v4l-subdev18` and `/bin/reboot-bootloader` IMEM reset verified live.
   - **Dash / VOOC Fast Charge MCU (`oneplus,dash-charger`, `4-0026`, `9a094c20e`):** Schematic + `dashd` REA confirmed `STM8S003F3U6TR` (`U4301`) behind `TXS0104` (`U4303`), `MAX14743` (`U4302`), and `MCU_EN` (`GPIO 96`). Clean-room `oneplus_dash.c` (`/dev/dash`, `power_supply/oneplus-dash`) verifies `oneplus/dash_stm8s.bin` (`5746` bytes, `ver 0x37`) in the `~1850ms` post-reset bootloader window while keeping `usb_switch=0` (`GPIO 90/91=0`).
4. **Remaining Non-Ported / Hardware-Gated Stock Items:**
   - **Widevine L1 / HDCP (`widevine.mdt`, `cppf.mdt`, `securemm.mdt`):** `hdcp*.mdt` absent from stock firmware partition, unlocked bootloader disables OEM keybox provisioning, and TZ XPU/secure SMMU boundaries block HLOS video buffers (Widevine L3 works).
   - **NXP P61 SPI eSE (`blsp1_spi1`) & Other-SKU Stubs (`fpc*`, `cpe_9340`, `a530_zap`, `slpi_v1`, `isdbtmm`):** Clean-room `nxp_p61_ese.c` (`b61b2af01`) built, but `GPIOs 0..3` (`blsp1_spi1`, `gpio-reserved-ranges = <0 4>`) are TZ XPU protected on production OnePlus 5T firmware (SError crash if accessed by HLOS), so `&blsp1_spi1` remains `disabled` in DT; standard NFC works via PN553 (`nfc0` I2C `3-0028`). Other-SKU stubs remain physically absent on `dumpling`.

---

## 5. Detailed Feasibility: Batch B, Batch C & Tier 3

### Batch B — Audio (`ADSP`, `WCD9335` SLIMbus, `TFA9890`) -> **Done & Verified on `#28` (CoolFlux DSP Bypass)**
- **Implemented & verified live on `#28`:** `CONFIG_SND_SOC_QDSP6`, `CONFIG_SND_SOC_MSM8998` (`sound/soc/qcom/msm8998.c`), `CONFIG_SND_SOC_WCD9335`, `CONFIG_SND_SOC_TFA989X`, `apr` / `q6afe` / `q6asm` / `q6adm` / `q6routing`, `slim@171c0000`, `div1_mclk: divclk1` at DT root (`/`, `PM8998 GPIO 13` `func2`, `RPM_SMD_DIV_CLK1`), `WCD9335` (`217:1a0:1:0` v2.0, `reset-gpios = <&tlmm 64 GPIO_ACTIVE_LOW>`), and `TFA9890` (`"nxp,tfa9890"`, `3-0036`, rev `0x80`).
- **Limits:** Upstream `tfa989x.c` runs `TFA9890` in CoolFlux DSP-bypass mode (`tfa989x_dsp_bypass()`, so EQ/protection from `tfa98xx.cnt` is bypassed); userspace AOSP 17 AIDL Audio HAL + ALSA UCM mixer paths remain to be integrated.

### Batch B — USB 2.0 OTG (`DRD`), PD PHY & USB 3.0
- **USB 2.0 OTG / DRD -> Done & Verified on `#28`:** `drivers/power/supply/qcom_smbx.c` wires PMI8998 Type-C (`0x1300`) + OTG VBUS boost (`0x1100`) + `usb-role-switch` (`/sys/class/typec/port0`, `[dual] source sink`, `a800000.usb-role-switch`) to `dwc3` (`dr_mode = "otg"`), with Dash switches (`gpio90, 91, 96`) locked `out low`.
- **USB PD PHY (`qcom,usb-pdphy@1700`) -> Likely portable (needs HW test):** Stock `stock_02.dts` sets `disable-pd;` on `qpnp-smb2`, while `qcom_pmic_typec_pdphy.c` (`0x40..0x80`) matches PMI8998 `pdphy@1700` offsets if PD is later needed.
- **USB 3.0 SuperSpeed (`5 Gbps`) -> Hardware Unsupported:** `msm8998-oneplus-common.dtsi` enforces `qusb2phy`, `qcom,select-utmi-as-pipe-clk`, `maximum-speed = "high-speed"`; SuperSpeed lanes are not wired on the Type-C receptacle.

### Batch C — Cameras (`IMX398`, `IMX376K`, `IMX371`, `LC898217XC`) -> **RAW10 V4L2 Capture Verified Live (`#33`); Stock ISP Blocked**
- **Implemented & verified live on `#33` (`4ac291598`):**
  - Clock tree (`gcc-msm8998.c` `gpll0_out_main_div2`, `mmcc-msm8998.c` `mclk0..3_clk_src` `.mnd_width = 8`, `camss_top_gdsc.pd.parent = &bimc_smmu_gdsc.pd`).
  - `i2c-qcom-cci.c` (`vio` regulator held across runtime PM window so shared `vreg_lvs1a_1p8` does not droop between back-to-back transactions, plus `cci_halt()` + IRQ clear on timeout recovery).
  - `camss` (`camss.c`, `camss-csiphy-3ph-1-0.c` `lane_regs_msm8998` for CSIPHY v5.0.1 10nm 2PH, `camss-csid-4-7.c`, `camss-ispif.c` v3.0, `camss-vfe.c` `genpd_link` refcounting across `vfe_get` & `ispif_reset`, `camss-vfe-4-8.c`).
  - Clean-room **`sony-imx-op5t.c`** driver (`CONFIG_VIDEO_SONY_IMX_OP5T=y`): all 3 sensors (`imx398` `0-001a`, `imx371` `0-0010`, `imx376k` `1-0010`) auto-probe at boot and stream full `SRGGB10P` RAW10 frames cleanly into userspace V4L2 buffers.
- **Remaining Batch C Follow-On:** Write clean-room V4L2 subdev driver + DT wiring for **`lc898217xc`** VCM autofocus actuators (`imx398`, `imx376k`) and `gt24c64` EEPROMs; integrate `libcamera` SoftISP + AOSP 17 Camera AIDL HAL. (Optional `vfe_res_8998` `PIX` resizer pipeline if HW scaling is needed beyond RDI.)
- **What CANNOT work on mainline `6.18`:** Stock `mm-qcamera-daemon`, `libchromatix_*.so` 3A/ISP pipelines, hardware CPP, and `libSonyIMX371RmscLibrary.so` remosaic require downstream `msm_isp` ioctls absent from mainline `camss`.

### Tier 3 — Long-Pole / Proprietary Blockers
- **Goodix `GF5216` Fingerprint -> Recoverable post-bring-up (requires `/dev/qseecom` port):** Stock `gf_spi.c` (`USE_PLATFORM_BUS = 1`, `AP_CONTROL_CLK` off) only drives GPIOs (`reset=tlmm 75`, `enable=tlmm 65`, `irq=tlmm 121`) + Netlink (`NETLINK_TEST = 25`), while SPI capture and matching execute inside QSEE TA `gfp5288.mdt` / `gfp5288d.mdt` (`APP_ID=0x111`). Cannot be written pure clean-room in HLOS, **but is recoverable** because `goodix.fingerprint.msm8998.so`, `libgf_hal.so`, and `libgf_ca.so` are all **`ELF 64-bit aarch64`** — port `goodix/`, a minimal `/dev/qseecom` (`qcom_scm` + CMA `dma-buf` + listener callbacks), `CONFIG_SCSI_UFS_BSG=y` RPMB, `libion` shim, and an AOSP 17 AIDL Biometrics wrapper (see [`docs/02_signed_firmware_and_security.md`](file:///home/zero/.gemini/antigravity/scratch/oneplus5t/docs/02_signed_firmware_and_security.md)).
- **Dash Charge (`STM8S003F3U6TR` @ `4-0026`) -> Clean-Room Kernel Driver Done (`9a094c20e`, live-verified):** Schematic (`U4301` `STM8S003F3U6TR` behind `TXS0104` `U4303`, `MAX14743` `U4302`, `MCU_EN=GPIO 96`) + `dashd` REA revealed an `~1850ms` I2C bootloader window at `0x26` (`100 kHz`, `ap_clk=0`) after `mcu_en` `1 -> 0` reset. Clean-room `drivers/power/supply/oneplus_dash.c` (`CONFIG_CHARGER_ONEPLUS_DASH=m`, `/dev/dash`, `power_supply/oneplus-dash`) verifies `oneplus/dash_stm8s.bin` (`5746` bytes, `ver 0x37`) at boot while keeping `usb_switch=0` (`GPIO 90/91=0`).
- **NXP P61 SPI eSE (`blsp1_spi1` GPIOs `0..3`) -> TZ XPU Protected:** Clean-room `drivers/nfc/nxp_p61_ese.c` (`b61b2af01`) built, but removing `<0 4>` from `&tlmm gpio-reserved-ranges` triggers an immediate early-boot TZ XPU SError reset into fastboot; `&blsp1_spi1` stays `status = "disabled"` in `msm8998-oneplus-common.dtsi` while `nfc0` (`1-0028` PN553) handles NFC.
- **Keymaster / Widevine / Play Integrity & Final Relock:** Software KeyMint, SW Gatekeeper, Widevine L3, and `MEETS_BASIC_INTEGRITY` work without QSEE; TZ Keymaster/Gatekeeper can be recovered via the `/dev/qseecom` + UFS BSG/RPMB + AIDL path. **Widevine L1 remains practically blocked** (needs TZ XPU `secure_region` + SMMU `qcom,secure-context-bank` + `msm_hdcp` + Venus/DPU CPZ handoff), and **Play Integrity `DEVICE`/`STRONG` cannot pass** cleanly (`YELLOW`/unlocked ROT + no StrongBox). Keep unlocked during bring-up; custom-signed `YELLOW` VB 1.0 relocking (`boot_signer` on `/boot` + `/recovery`, `fastboot oem lock`) is an end-of-development hardening step only (see [`docs/02_signed_firmware_and_security.md`](file:///home/zero/.gemini/antigravity/scratch/oneplus5t/docs/02_signed_firmware_and_security.md#L100-L127)).
