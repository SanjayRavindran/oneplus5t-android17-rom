# Clean-Room Specification: Camera, Audio & Display Subsystems (`MSM8998` / `dumpling`)

## 1. Camera Subsystem (`CAMSS` + Sony IMX398, IMX376K, IMX371)

Derived from `rea decompile-all` and `.data` structural dissection of `/vendor/lib/libmmcamera_imx398.so`, `libmmcamera_imx376k.so`, `libmmcamera_imx371.so`, and `/vendor/etc/camera/camera_config.xml`:

### A. Sensor Hardware & CSI-2 Topology Matrix

| Parameter | Rear Primary (`CameraId = 0`) | Rear Secondary Low-Light (`CameraId = 1`) | Front Selfie (`CameraId = 2`) |
| :--- | :--- | :--- | :--- |
| **Sensor Chip** | **Sony IMX398** (`16.28 MP` PDAF) | **Sony IMX376K** (`20.11 MP`) | **Sony IMX371** (`16.09 MP`) |
| **I2C Slave Address** | `0x34` (8-bit) / **`0x1A` (7-bit)** | `0x20` (8-bit) / **`0x10` (7-bit)** | `0x20` (8-bit) / **`0x10` (7-bit)** |
| **I2C Bus Mode & Reg Width** | Fast Mode (`400 kHz`), 16-bit Addr | Fast Mode (`400 kHz`), 16-bit Addr | Fast Mode (`400 kHz`), 16-bit Addr |
| **Chip ID Reg & Expected ID** | Reg `0x0016` = **`0x0398`** (mask `0xFFF0`) | Reg `0x0016` = **`0x0376`** | Reg `0x0016` = **`0x0371`** (`0x0301` in lib, live silicon `0x0371`, mask `0xFFF0`) |
| **MCLK Frequency** | **`24,000,000 Hz` (`24 MHz`)** | **`24,000,000 Hz` (`24 MHz`)** | **`24,000,000 Hz` (`24 MHz`)** |
| **MIPI CSI-2 Receiver** | `CSIDCore = 0`, 4 Data Lanes (`LaneMask = 0x1F`, `LaneAssign = 0x4320`) | `CSIDCore = 1`, 4 Data Lanes (`LaneMask = 0x1F`, `LaneAssign = 0x4320`) | `CSIDCore = 2`, 4 Data Lanes (`LaneMask = 0x1F`, `LaneAssign = 0x4320`) |
| **Active Pixel Array** | **`4656 x 3496`** (`0x1230 x 0x0DA8`) | **`5184 x 3880`** (`0x1440 x 0x0F28`) | **`4656 x 3456`** (`0x1230 x 0x0D80`) |
| **Mount Angle & Optics** | `90°`, `f/1.7`, Focal Length `4.103 mm` | `90°`, `f/1.7`, Focal Length `4.103 mm` | `270°`, `f/2.0`, Focal Length `3.519 mm` |
| **VCM Voice-Coil Actuator** | ON Semi **`lc898217xc_imx398`** | ON Semi **`lc898217xc_imx376k`** | Fixed Focus (None) |
| **Calibration EEPROM** | **`imx398_gt24c64`** + `/mnt/vendor/persist/dual_camera_calibration/` | **`imx376k_gt24c64`** | **`sony_imx371`** |

### B. Common Sony IMX Register Map & Exposure/Gain Formulas (`rea` Decompilation)

All three sensors (`IMX398`, `IMX376K`, `IMX371`) share the Sony IMX MIPI CCS-like register map verified in `.data` offset `+0x1ec` and `sensor_fill_exposure_array()` (`0x12130`):
- **`0x0100`**: Mode Select (`0x00` = Software Standby, `0x01` = Streaming)
- **`0x0340` / `0x0341`**: `frame_length_lines` (`FLL` MSB / LSB)
- **`0x0342` / `0x0343`**: `line_length_pclk` (`LLP` MSB / LSB)
- **`0x034C` / `0x034D`**: `x_output_size` (MSB / LSB)
- **`0x034E` / `0x034F`**: `y_output_size` (MSB / LSB)
- **`0x0202` / `0x0203`**: `coarse_integration_time` (`CIT` MSB / LSB)
- **`0x0204` / `0x0205`**: `ana_gain_global` (`AG` MSB / LSB):
  $$\text{reg\_gain} = \lfloor 512 - \frac{512}{\text{analog\_gain}} \rfloor, \quad \text{clamped to } [0, 448] \ (1.0\times \text{ to } 8.0\times)$$
- **`0x020E`–`0x0215`**: Digital gain per Bayer channel (`Gr`, `R`, `B`, `Gb`, 16-bit Q8 fixed-point where `0x0100` = `1.0x`)
- **`0x3002`**: Long-exposure / Slow-Shutter bit-shift register (`0` for normal, `1` for `CIT >> 1`, `4` for `CIT >> 4`, `6` for `CIT / 64.07`).

### C. Mainline `6.18` `camss` Implementation & Verified `#33` Status
- **Implemented & Verified Live (`#33`, commit `4ac291598`):**
  - **Clock & Power Domains:** `gcc-msm8998.c` (`gpll0_out_main_div2`), `mmcc-msm8998.c` (`mclk0..3_clk_src` `.mnd_width = 8`, `camss_top_gdsc.pd.parent = &bimc_smmu_gdsc.pd`), and `msm8998.dtsi` (`cci@ca0c000` `mnoc_ahb`/`smmu_ahb`/`smmu_axi`).
  - **`i2c-qcom-cci.c` Reliability:** Added optional `"vio"` regulator (`vreg_lvs1a_1p8`) held while CCI is runtime-active (prevents pull-up droop within the `1000ms` autosuspend window between sequential probes/switches), plus `cci_halt()` + `CCI_IRQ_CLEAR_0`/`CCI_IRQ_GLOBAL_CLEAR_CMD` on recovery.
  - **`qcom-camss` MSM8998 Bring-Up:** Added `CAMSS_8998` support in `camss.c`, `camss-csiphy-3ph-1-0.c` (`lane_regs_msm8998` for 10nm 2PH v5.0.1 incl. `+0x08` settle count and `+0x24=0x04` control writes), `camss-csid-4-7.c` (`vfe_optional_parent_dev_ops` for `csid2`/`csid3` beyond 2 VFEs), `camss-ispif.c` v3.0, and `camss-vfe.c`/`camss-vfe-4-8.c` (`genpd_lock` + `genpd_count` refcounting so `ispif_reset()` does not tear down `vfe0->genpd_link` mid-stream).
  - **Clean-Room Sensor Driver (`drivers/media/i2c/sony-imx-op5t.c`):** All 3 sensors identify cleanly (`0x0398`, `0x0371`, `0x0376`) and capture real `SRGGB10P` RAW10 frames (`4656x3496` `/dev/video0`, `4656x3456` `/dev/video1`, `2592x1940` `/dev/video3`) with clean `STREAMOFF`.
- **Clean-Room VCM & EEPROM + Remaining Work:** `lc898217xc-op5t` VCM actuators (`1-0074`, `0-0072`) and `24c64` EEPROMs (`eb7a51cc2`) are ported and `lc898217xc` `/dev/v4l-subdev18` (`0..1023`) is verified live (`#51`); integrate `libcamera` SoftISP (`RDI0..2`) + Evolution X Camera AIDL HAL. (`vfe_res_8998` `PIX` resizer pipeline is optional/unwired.)
- **Cannot Run on Mainline `6.18`:** Proprietary `mm-qcamera-daemon`, `libchromatix_*.so` 3A/ISP pipelines, hardware CPP post-processing, and `libSonyIMX371RmscLibrary.so` remosaic require downstream `msm_isp` ioctls absent upstream.

---

## 2. Audio Subsystem (`QDSP6` + `WCD9335` Tasha + `TFA9890`) — **Verified on `#28`**

Derived from `rea decompile-all` on `stock_batchb_audio_usb.elf` (`8,987` functions), `audio.primary.msm8998.so` (`1,780` functions), `libaudioalsa.so` (`57` functions), `stock_02.dts`, `adb_dump/etc/`, `tfa98xx.cnt` (`4,196` bytes), and **verified live on `#28`**:

> **Important Silicon Fact (`stock_02.dts` + `adb_dump` + Live Hardware Proof):** Although `stock_02.dts` contains both `sound-tavil` (`WCD9340`) and `sound-9335` (`WCD9335`) nodes for multi-project DTS sharing, OnePlus 5T (`17801`, `hw_version=43`) places `op,project_17801;` inside **`sound-9335`** (`msm8998-tasha-snd-card`), confirmed by `adb_dump/etc/ftm_test_config_msm8998-tasha-snd-card`. Likewise, I2C `3-0036` reads hardware revision **`0x80` (`TFA9890`)** (`"TFA9890"` in `tfa98xx.cnt`), **not** `0x81` (`TFA9891`).

- **Audio DSP:** Hexagon ADSP (`adsp.mbn` -> `remoteproc1` at `0x17300000`, `running`) + PDR (`msm/adsp/audio_pd`, `0x1fffffff`) + `qcom,apr` (`aprsvc:service:4:{3,4,7,8}`: `q6core`, `q6asm-dai`, `q6afe-dai`, `q6adm`/`q6routing`).
- **Primary Codec — Qualcomm `WCD9335` ("Tasha", `217:1a0:1:0`, verified `WCD9335 CODEC version is v2.0` on `#28`):**
  - **SLIMbus & BAM:** `slim@171c0000` (`reg = <0x171c0000 0x2c000>`, IRQ `GIC_SPI 163`, TLMM `gpio70..72` `lpass_slimbus`) + `slimbam@17184000`, matching `qcom,slim-ngd-v1.5.0`. Elemental addresses `[00 00 a0 01 17 02]` (`tasha_ifd@0,0`, `"slim217,1a0"`) and `[00 01 a0 01 17 02]` (`codec@1,0`, `"slim217,1a0"`).
  - **Clocks, Reset, IRQ & Rails:**
    - `mclk`: **`div1_mclk: divclk1`** (`compatible = "gpio-gate-clock"` placed directly under DT root `/` so `of_platform_default_populate` instantiates it as a platform device!), backed by `RPM_SMD_DIV_CLK1` gated via **`pm8998_gpios 13` (`func2`, `power-source = <0>`)** (`clock-frequency` derived from XO, `wcd9335` sets `9.6 MHz`).
    - `slimbus` clock: **`<&rpmcc RPM_SMD_XO_CLK_SRC>`**.
    - Codec interrupt on **TLMM `GPIO 54`** (`IRQ_TYPE_LEVEL_HIGH`), reset GPIO on **TLMM `GPIO 64` (`GPIO_ACTIVE_LOW`)** (`wcd9335_power_on_reset()` drives logical high for 20 ms then deasserts low -> physical `out high`), and 5 static supplies (`vdd-buck`, `vdd-buck-sido`, `vdd-tx`, `vdd-rx`, `vdd-io`) powered by **`vreg_s4a_1p8` (`1.8V`)**.
- **Smart Speaker Amplifier — NXP `TFA9890` (`tfa9890@36`, rev `0x80`) on `blsp2_i2c3` (`0x0c1b7000`) + `QUAT_MI2S` (`GPIO 58..61`):**
  - Binds cleanly in mainline `6.18` (`tfa989x 3-0036`, `"nxp,tfa9890"`, `tfa989x-hifi` DAI) using `vreg_s4a_1p8` (`1.8V` logic) and `tfa989x_dsp_bypass()` (matching the `"bypass"` profile decoded from `tfa98xx.cnt` `0x1051..0x1063`: `CHSA=0`, `CFE=0`, `DCA=0`). CoolFlux DSP EQ/protection profiles (`"music_hq"`, `"speech"`) remain unimplemented until/unless DSP firmware loading is added.
- **Mainline `6.18` Live Verification (`#28`):**
  - `cat /sys/kernel/debug/devices_deferred` -> empty (`0` deferred devices).
  - `cat /proc/asound/cards` -> `0 [O5T]: msm8998-snd - OnePlus 5T` (`MultiMedia1..6` PCMs, `/dev/snd/{controlC0,pcmC0D0..5{c,p},timer}`).
  - ASoC components bound: `217:1a0:1:0` (`wcd9335`), `tfa989x.3-0036`, `apr:service@{4,7,8}` (`q6asm`, `q6afe`, `q6routing`).
- **AOSP 17 Next Step:** Implement Android 17 AIDL Audio HAL (`android.hardware.audio.core.IModule`) over TinyALSA + ALSA UCM mixer paths for `WCD9335` (`SLIMBUS_0_RX/TX`) and `TFA9890` (`QUAT_MI2S_RX` DSP-bypass).

---

## 3. USB 2.0 OTG, Type-C Role Switching, `PDPHY` & Dash Switch Protection

Derived from `rea decompile-all` on `stock_batchb_audio_usb.elf`, `android.hardware.usb@1.0-service.oneplus5.so`, `dashd`, and `stock_02.dts`:

- **USB Controller & PHY Baseline (Already Working in Peripheral Mode):** `ssusb@a800000` + `qusb2phy` (`c012000`), configured in `msm8998-oneplus-common.dtsi` as `maximum-speed = "high-speed"` (`qcom,select-utmi-as-pipe-clk;`, no USB 3 SuperSpeed lanes wired on OnePlus 5T) and currently pinned to `dr_mode = "peripheral"` (`/* Force to peripheral until we can switch modes */`).
- **Critical Hardware Safety Interlock — `oneplus,dash-charger@26` (`STM8S003F3U6TR`) USB D+/D- MUX (`stock_batchb_decomp` + schematics):**
  - On `blsp2_i2c1` (`0x26`, `100 kHz`), the Dash Charge `STM8S003F3U6TR` (`U4301`, behind `TXS0104` `U4303`) shares USB D+/D- through `MAX14743` (`U4302`) controlled by **TLMM `GPIO 90` (`usb-sw-1-gpio`)**, **TLMM `GPIO 91` (`usb-sw-2-gpio`)**, and `Q4301` **TLMM `GPIO 96` (`mcu-en-gpio`)**.
  - Normal MSM8998 `qusb2phy` D+/D- data routing drives `usb-sw` LOW (`90=0, 91=0`). Clean-room `oneplus_dash.c` (`9a094c20e`) holds `usb_switch=0` across probe/verification, pulsing `mcu-en` (`1 -> 0` reset) with `ap_clk=0` (`GPIO 79`) to verify `oneplus/dash_stm8s.bin` (`ver 0x37`, `5746` bytes) in the `~1850ms` I2C bootloader window without disturbing USB gadget connectivity.
- **PMI8998 Type-C Port & VBUS Boost (`smb2@1000..16ff` vs mainline `drivers/power/supply/qcom_smbx.c`):**
  - Stock `qcom,qpnp-smb2` places Type-C state machine in **`chgr-usbin@1300`** (`"type-c-change"` IRQ `0x13:7`, reading `0x130B..0x130F`, configuring `0x1358..0x1368`) and VBUS host boost in **`otg@1100`** (`0x1140` OTG enable, `0x1109` status bit 3, `0x11c0`/`0x11d0=0xa5`), plus role-swap MISC registers **`0x16d0 = 0xa5`**, **`0x16e9 = 0xa5 / 0`**, and `0x1368[2:0]` (`0`=DRP, `1`=Disable, `2`=Source/DFP, `4`=Sink/UFP) in `smblib_set_prop_typec_power_role`.
  - **Mainline `6.18` Gap:** `drivers/power/supply/qcom_smbx.c` already defines all `0x130B..0x1368` and `0x1153` register constants, **but currently only exposes battery/USBIN charger properties** (`cable_irq` only) and does NOT wire `type-c-change` IRQ, an `otg-vbus` regulator (`0x1140`), or `usb-role-switch` (`typec_port`) to `usb3_dwc3`.
- **PMI8998 `pdphy@1700` vs Mainline `qcom_pmic_typec_pdphy.c`:**
  - REA of `pd_phy_open`, `pd_phy_write`, `pdphy_msg_rx_irq`, `pd_phy_signal`, `pd_phy_update_roles` proves PMI8998 `0x1700` registers (`0x40` MSG_CONFIG, `0x42` TX_SIZE, `0x44` TX_CTRL, `0x46` EN_CTRL, `0x48` RX_SIZE, `0x4A` RX_STATUS, `0x4B` RX_ACK, `0x4C` FRAME_FILTER, `0x4E` BIST, `0x60..0x80` buffers) and 7 IRQs are **100% register-identical** to `pm8150b_pdphy_res` in `drivers/usb/typec/tcpm/qcom/qcom_pmic_typec_pdphy.c` (`vdd-pdphy` = `vreg_l24a_3p075`). Note stock `stock_02.dts` sets `disable-pd;` on `qpnp-smb2` and only uses Type-C CC/OTG (`0x1300`/`0x1100`), with PDPHY unused for charging.
- **Implemented & Verified Live on `#28` (Batch B — USB):**
  - `qcom_smbx.c` extended (clean-room) with PMI8998 Type-C (`0x1300`) IRQ/role reporting + `0x1140` OTG VBUS boost regulator + `typec_port` / `usb-role-switch` (`CONFIG_TYPEC`, `CONFIG_USB_ROLE_SWITCH`, `CONFIG_USB_DWC3_DUAL_ROLE=y`) wired to `dwc3` (`dr_mode = "otg"`, `a800000.usb-role-switch`), with `TLMM GPIO 90, 91, 96` locked `out low`.
  - Live `#28` check: `/sys/class/typec/port0` (`port_type = [dual] source sink`, `data_role = host [device]`, `power_role = source [sink]`) and `gpio90/91/96` all `out low func0 2mA no pull`.
  - **AOSP 17 Next Step:** Implement AOSP 17 **AIDL `android.hardware.usb.IUsb`** reading/writing `/sys/class/typec/` + ConfigFS USB gadget (`MTP`, `ADB`, `RNDIS`/`NCM`, `MIDI`).

---

## 4. Display, Touch & Graphics Subsystem (`Samsung S6E3FC1` + `S3508` + `Adreno 540`)

**Verified live in kernel `#28`:**
- **Display Panel:** **Samsung `S6E3FC1`** (`1080 x 2160` command-mode AMOLED, `dsi_samsung_s6e3fc1_cmd_display`) on **`msm_dpu` (`DPU 3.0`, `c901000.display-controller`)**, exposing `/dev/dri/card0` (`card0-DSI-1` `connected`, `1080x2160`, backlight `1023/1023`).
- **Touchscreen Controller:** **Synaptics `S3508`** (`0-0020`, `Synaptics s3508` input device).
- **GPU:** **Qualcomm Adreno 540 (`5000000.gpu`)** loading `a530_pm4.fw`, `a530_pfp.fw`, `a540_gpmu.fw2`, and signed `a540_zap.mbn`, exposing `/dev/dri/renderD128`.
- **Open-Source AOSP 17 Userspace (next step):** Integrate **Mesa3D `freedreno` / `turnip`** + GBM gralloc + `drm_hwcomposer` (`IComposer3` AIDL).
