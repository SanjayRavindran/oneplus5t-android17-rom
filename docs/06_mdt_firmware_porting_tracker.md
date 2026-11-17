# OnePlus 5T (`dumpling`) `.mdt` Signed Firmware & QSEE TA Porting Tracker (`Linux 6.18 LTS`)

> **Scope:** Complete audit of all **33** `.mdt` signed firmware images present on stock OnePlus 5T (`30` in `firmware_raw/modem/image/` and `3` in `firmware_raw/vendor_fw/`), plus UFS TrustZone applet (`.mbn` / ELF-split) partitions (`keymaster`, `cmnlib`, `cmnlib64`, `storsec`), helper `.mbn` images (`mba.mbn`, `wlanmdsp.mbn`), and our prioritized porting roadmap onto `dumpling-6.18-android17`.

---

## 1. Key Architectural Findings

1. **One Shared Kernel Loader Unlocks Most Remaining `.mdt`s (`/dev/qseecom`):**
   - Unlike MPSS, ADSP, SLPI, Venus, IPA, and Adreno ZAP (which each have dedicated `remoteproc` / IOMMU-mapped peripheral loaders), **most remaining `.mdt` files are TrustZone QSEE Trusted Applications (TAs)** executing inside TrustZone (`tz`), not standalone peripheral CPUs.
   - Userspace (`vendor.qseecomd`, Keymaster HAL, Gatekeeper HAL, Goodix FP HAL, etc.) loads them through a **single kernel driver**: `/dev/qseecom` (`stock-oneplus-msm8998/drivers/misc/qseecom.c`, `9,457` lines), backed by `qcom_scm_call()`, CMA (`qseecom_region` `0x1400000` / `secapp-region` `0x86600000 0x2200000`), and **RPMB** listener support (`CONFIG_SCSI_UFS_BSG=y`).
2. **Real Identity of `cppf.mdt`:**
   - Inspection of `.bXX` segments via ELF/string analysis shows `cppf.mdt` (`ELF64 AArch64`, `38 KiB`, `APP_ID 0xAAA`) is **not** Camera CPP microcontroller firmware. Its symbols (`cppf_cmd_handler`, `cppf_MinEncLevelTestGet`, `crypto interfaces`) show it is a QSEE Content Protection Path Enforcement DRM TA.
3. **Only Two Dedicated Non-`qseecom` Kernel Drivers Are Actually Needed for Active `dumpling` Hardware:**
   - **Goodix `GF5216` Fingerprint Bridge** (`stock-oneplus-msm8998/drivers/input/fingerprint/goodix/`: `gf_spi.c`, `gf_spi.h`, `platform.c`, `netlink.c`, `1,342` lines total) — provides reset/power/IRQ GPIO control and netlink event delivery (`gfp5288.mdt` runs SPI in TEE via QSEE).
   - **WCD9335 Tasha CPE (`cpe_9335.mdt`)** (`stock-oneplus-msm8998/sound/soc/codecs/wcd9335.c`, `wcd_cpe_core.c`, `wcd_cpe_services.c`) — optional Xtensa DSP loader for always-on hardware voice wakeup / SoundTrigger.
4. **Why Some Blobs Are Marked "Blocked Beyond `/dev/qseecom`" or "Skip (Other-SKU / Test)":**
   - **DRM L1 (`widevine.mdt`, `cppf.mdt`, `securemm.mdt`):** Even with `/dev/qseecom` working, full L1 protected video playback also requires TZ XPU `secure_region` (`0x5c00000`), secure SMMU context banks, CPZ paths in `venus` + `msm_dpu`, and HDCP.
   - **Dead / Other-SKU / Internal Test Images:** OnePlus/Qualcomm ships shared image directories containing FPC fingerprint TAs (wrong sensor vendor — `dumpling` uses Goodix), WCD9340 `cpe_9340.mdt` (`dumpling` uses WCD9335), `slpi_v1.mdt` (`dumpling` uses `slpi_v2.mdt`), `a530_zap.mdt` (`dumpling` uses Adreno 540 `a540_zap.mdt`), Japanese `isdbtmm.mdt` (no ISDB-T tuner), and Qualcomm sample/test apps (`gptest.mdt`, `smplap32.mdt`, `smplap64.mdt`, `dhsecapp.mdt`).

---

## 2. Port Status Summary

| Category | Count | Items | Action |
| :--- | :---: | :--- | :--- |
| **Running / Verified on Linux `6.18` (`#52`)** | **8** `.mdt` (+2 `.mbn`) | `modem`, `adsp`, `slpi_v2`, `venus`, `ipa_fws`, `a540_zap`, `cmnlib`, `cmnlib64` (+ `mba.mbn`, `wlanmdsp.mbn`) + `cpe_9335.mdt` (`9` active!) | **Done (`#52` live)** |
| **Kernel Support Ported (`#52`); Ready for Userspace HALs** | **8** `.mdt` + UFS TAs | `gfp5288`, `gfp5288d`, `faceapp`, `soter64`, `alipay`, `qmpsecap`, `mdtpseca` + UFS `keymaster` / `cmnlib*` / `storsec` | Kernel Tracks 1–3 Done; wire Evolution X AIDL HALs |
| **DRM L1 Blocked Beyond `/dev/qseecom`** | **3** `.mdt` | `widevine`, `cppf`, `securemm` | Loads via Track 1; full L1 blocked (`hdcp*.mdt` missing, unlocked BL, XPU) |
| **Skip — Other-SKU / Legacy / Test-Only** | **14** `.mdt` | `fpc1035*`, `fpc1245*`, `fpc1263*` (6), `cpe_9340`, `slpi_v1`, `a530_zap`, `isdbtmm`, `dhsecapp`, `gptest`, `smplap32`, `smplap64` | Documented & Skipped |

*(Exact accounting: `6` Running + `10` Port Targets + `3` Blocked Beyond `/dev/qseecom` + `14` Other-SKU/Test = **`33` `.mdt` files**.)*

---

## 3. Master Inventory of All 33 `.mdt` Files & UFS TA Partitions

### 3A. Already Active on `Linux 6.18` (`6` `.mdt` + `2` `.mbn`)

| Firmware | Location | ELF Class / Machine | Size | Purpose | `6.18` Status |
| :--- | :--- | :--- | ---: | :--- | :--- |
| `modem.mdt` (+ `mba.mbn`) | `modem/image/` | `ELF32` QDSP6 | `55,362 KiB` | MPSS Hexagon modem (`4080000.remoteproc`, `qcom_q6v5_mss`) | **Running** (`#52`) |
| `wlanmdsp.mbn` | `modem/image/` | Hexagon MBN | Helper | WCN3990 Wi-Fi firmware via `ath10k_snoc` + MPSS MSA | **Running** (`#52`) |
| `adsp.mdt` | `modem/image/` | `ELF32` QDSP6 | `9,978 KiB` | Audio/Voice QDSP6 (`remoteproc1`, `qcom_q6v5_pas`) | **Running** (`#52`) |
| `slpi_v2.mdt` | `modem/image/` | `ELF32` QDSP6 | `4,256 KiB` | Sensor Low Power Island QDSP6 (`remoteproc0`, `qcom_q6v5_pas`) | **Running** (`#52`) |
| `venus.mdt` | `modem/image/` | `ELF32` ARM | `944 KiB` | Venus 4.2 video encode/decode microcontroller (`venus-core`) | **Running** (`#52`) |
| `ipa_fws.mdt` | `vendor_fw/` | `ELF32` QDSP6 | `59 KiB` | IPA v3.1 packet accelerator microcontroller (`ipa`) | **Running** (`#52`) |
| `a540_zap.mdt` | `vendor_fw/` | `ELF32` QDSP6 | `34 KiB` | Adreno 540 GPU microcode / ZAP shader (`msm`) | **Running** (`#52`) |

### 3B. Porting Targets — Usable TAs & Hardware DSP (`10` `.mdt` + UFS Partitions)

| Firmware | Location | ELF Class / Machine | Size | `APP_ID` / Notes | Purpose & Observed Evidence | Track & Dependencies | Status |
| :--- | :--- | :--- | ---: | :--- | :--- | :--- | :---: |
| `cmnlib.mdt` | `modem/image/` | `ELF32` ARM | `211 KiB` | `0x555` | QSEE 32-bit common shared library (`qsee_malloc`, QSEE Request Service). Required by 32-bit TAs. Also in UFS `cmnlib` (`sde27`/`sde29`). | **Track 1**: `/dev/qseecom` (`qcom_scm_qseecom_raw` + CMA) | `[x]` |
| `cmnlib64.mdt` | `modem/image/` | `ELF64` AArch64 | `280 KiB` | `0x555` | QSEE 64-bit common shared library. Required by 64-bit TAs. Also in UFS `cmnlib64` (`sde28`/`sde30`; DT `qcom,commonlib64-loaded-by-uefi`). | **Track 1**: `/dev/qseecom` | `[x]` |
| `gfp5288.mdt` | `modem/image/` | `ELF32` ARM | `1,698 KiB` | `0x111` | Goodix `GF5216`/`GF5288` Fingerprint TA (`goodix5288/src/gf_lib/common/gf_secure_object`, `/data/system/users/0/fpdata/fingerprint_`). | **Track 1** (`/dev/qseecom` `app_id=131074`) + **Track 2** (`goodix_op5t` `/dev/goodix_fp`, `0x03 gfp5288`, ioctls verified `#51`/`#52`) + FP AIDL HAL | `[x]` Kernel |
| `gfp5288d.mdt` | `modem/image/` | `ELF32` ARM | `1,799 KiB` | Companion | Goodix `GF5288` fingerprint companion/variant TA image. | **Track 1** + **Track 2** (kernel complete) | `[x]` Kernel |
| `cpe_9335.mdt` | `modem/image/` | `ELF32` Tensilica Xtensa | `72 KiB` | `CPE_BUILD_VERSION 1.0` | WCD9335 Tasha Codec Processing Engine FW (`APPS Wakeup latency`; loaded via clean-room `wcd9335-cpe.c` + `regmap` SIDO). | **Track 3**: `wcd9335-cpe` (`running=1 cmi_ver=1 sfr=0x4c690/80 shmem=0x56580 errs=0`, `0->1` reload verified `#51`/`#52`) | `[x]` |
| `faceapp.mdt` | `modem/image/` | `ELF32` ARM | `46 KiB` | `0x12346` | OnePlus Face Unlock QSEE TA (`/data/misc/qsee/sampleapp/fts/face.dat`; `vendor.oneplus.faceunlock.hal@1.0-service.rc`). | **Track 1**: `/dev/qseecom` (TA load verified; `app_id=196610`) + camera + Face HAL | `[~]` |
| `soter64.mdt` | `modem/image/` | `ELF64` AArch64 | `163 KiB` | `0x216` | Tencent WeChat Soter biometric attestation TA (`generate_attk_key_pair`, `construct_soter_sign_data`; `vendor.qti.hardware.soter@1.0-service.rc`). | **Track 1**: `/dev/qseecom` (TA load verified; `app_id=262147`) + RPMB | `[x]` |
| `alipay.mdt` | `modem/image/` | `ELF32` ARM | `133 KiB` | `0x111` | Alipay IFAA biometric authentication TA (`init_alipay_auth_result`, `[IFAA]`). | **Track 1**: `/dev/qseecom` + RPMB | `[ ]` |
| `qmpsecap.mdt` | `modem/image/` | `ELF32` ARM | `1,000 KiB` | `0x822E8E918CB64827` | Qualcomm Modem/RF Security TA (`Apprfmstate->numStumpsInRFMs`). | **Track 1**: `/dev/qseecom` | `[ ]` |
| `mdtpseca.mdt` | `modem/image/` | `ELF32` ARM | `1,047 KiB` | `0x0B7BC69253EF018A` | Mobile Device Theft Protection (MDTP) QSEE TA (`mdtp_get_state_impl`, uses RPMB). | **Track 1**: `/dev/qseecom` + `CONFIG_SCSI_UFS_BSG` | `[ ]` |
| *(UFS)* `keymaster` (`sde25`) | UFS partition | QSEE TA (`512 KB`) | — | — | Hardware Keymaster 3.0 & Gatekeeper TA (`android.hardware.keymaster@3.0-service.rc`, `android.hardware.gatekeeper@1.0-service.rc`). | **Track 1**: `/dev/qseecom` + `CONFIG_SCSI_UFS_BSG` + `qseecomd` | `[ ]` |

### 3C. Blocked Beyond `/dev/qseecom` — DRM L1 / Secure Display Path (`3` `.mdt`)

| Firmware | Location | ELF Class / Machine | Size | `APP_ID` | Purpose & Observed Evidence | What Is Needed Beyond Track 1 | Status |
| :--- | :--- | :--- | ---: | :--- | :--- | :--- | :---: |
| `widevine.mdt` | `modem/image/` | `ELF64` AArch64 | `362 KiB` | `0x333` | Widevine L1 OEMCrypto QSEE TA (`OEMCrypto_EncryptAndStore`). | TA loading works via Track 1, **but L1 playback** also requires TZ XPU `secure_region` (`0x5c00000`), secure SMMU CBs, `venus`/`msm_dpu` CPZ, and HDCP (**Track 4**). | `[ ]` |
| `cppf.mdt` | `modem/image/` | `ELF64` AArch64 | `38 KiB` | `0xAAA` | QSEE Content Protection Path Enforcement TA (`cppf_cmd_handler`, `cppf_MinEncLevelTestGet`, `crypto interfaces`). *(Not Camera CPP.)* | Same secure-path constraints as `widevine.mdt` (**Track 4**). | `[ ]` |
| `securemm.mdt` | `modem/image/` | `ELF64` AArch64 | `194 KiB` | `0x2764296DB56BAACC` | QSEE Secure Multimedia TA (`SECURE_MM App Init`, `General Use Test Key` string). | Same secure-path constraints (**Track 4**). | `[ ]` |

### 3D. Dead / Other-SKU / Internal Test Images (`14` `.mdt` — Do Not Port)

| Firmware | Location | ELF Class / Machine | Size | `APP_ID` / Notes | Reason Skipped on OnePlus 5T (`dumpling`) | Status |
| :--- | :--- | :--- | ---: | :--- | :--- | :---: |
| `fpc1035.mdt` | `modem/image/` | `ELF32` ARM | `579 KiB` | `0x111` | FPC fingerprint TA for non-dumpling SKU (`dumpling` has Goodix `GF5216`). | `N/A (Skip)` |
| `fpc1035d.mdt` | `modem/image/` | `ELF32` ARM | `641 KiB` | `0x111` | FPC1035 companion TA (wrong fingerprint vendor). | `N/A (Skip)` |
| `fpc1245.mdt` | `modem/image/` | `ELF32` ARM | `597 KiB` | `0x111` | FPC1245 fingerprint TA (wrong fingerprint vendor). | `N/A (Skip)` |
| `fpc1245d.mdt` | `modem/image/` | `ELF32` ARM | `643 KiB` | `0x111` | FPC1245 companion TA (wrong fingerprint vendor). | `N/A (Skip)` |
| `fpc1263.mdt` | `modem/image/` | `ELF32` ARM | `589 KiB` | `0x111` | FPC1263 fingerprint TA (wrong fingerprint vendor). | `N/A (Skip)` |
| `fpc1263d.mdt` | `modem/image/` | `ELF32` ARM | `654 KiB` | `0x111` | FPC1263 companion TA (wrong fingerprint vendor). | `N/A (Skip)` |
| `cpe_9340.mdt` | `modem/image/` | `ELF32` Xtensa | `250 KiB` | `wcd9340` | WCD9340 Tavil CPE FW from shared DTS (`stock_02.dts` multi-SKU snippet); `dumpling` uses `wcd9335`. | `N/A (Skip)` |
| `slpi_v1.mdt` | `modem/image/` | `ELF32` QDSP6 | `4,228 KiB` | Legacy | Legacy SLPI v1 image; `dumpling` hardware uses `slpi_v2.mdt`. | `N/A (Skip)` |
| `a530_zap.mdt` | `vendor_fw/` | `ELF32` QDSP6 | `34 KiB` | Adreno 530 | MSM8996 Adreno 530 ZAP; `dumpling` uses Adreno 540 `a540_zap.mdt`. | `N/A (Skip)` |
| `isdbtmm.mdt` | `modem/image/` | `ELF32` ARM | `118 KiB` | `0x222` | Japanese ISDB-T mobile TV BKM DRM TA; `dumpling` has no ISDB-T tuner. | `N/A (Skip)` |
| `dhsecapp.mdt` | `modem/image/` | `ELF64` AArch64 | `142 KiB` | `General Use Test Key` | Qualcomm internal Diffie-Hellman / crypto test TA. | `N/A (Skip)` |
| `gptest.mdt` | `modem/image/` | `ELF32` ARM | `391 KiB` | `/persist/data/sampleapp/gptestfile300k` | GlobalPlatform TEE internal test applet. | `N/A (Skip)` |
| `smplap32.mdt` | `modem/image/` | `ELF32` ARM | `1,538 KiB` | `SAMPLEApp Init` | Qualcomm 32-bit QSEE sample/reference TA. | `N/A (Skip)` |
| `smplap64.mdt` | `modem/image/` | `ELF64` AArch64 | `1,607 KiB` | `SAMPLEApp Init` | Qualcomm 64-bit QSEE sample/reference TA. | `N/A (Skip)` |

---

## 4. Prioritized Porting Plan & Verification Checklist

### Track 1 — `/dev/qseecom` + RPMB (`UFS_IOCTL_QUERY` + `CONFIG_CHR_DEV_SG` + `CONFIG_SCSI_UFS_BSG`)
- **Stock & Kernel Implementation:**
  - `3deb9bd3d` (`ufs: add UFS_IOCTL_QUERY and enable CONFIG_CHR_DEV_SG for RPMB`): added `UFS_IOCTL_QUERY` (`0x5388`) on `/dev/sg*` + `CONFIG_CHR_DEV_SG=y` alongside `CONFIG_SCSI_UFS_BSG=y`.
  - `d7f11f9cd` (`misc: qseecom: add /dev/qseecom driver, qcom_scm bridge, and msm8998 DT region`): added `qcom_scm_qseecom_raw()` / `qcom_scm_tzmem_{alloc,free}()` bridge, `drivers/misc/qseecom.c` (`CONFIG_QCOM_QSEECOM_DEV=y`), `qseecom_region` (`32 MiB` `< 4 GiB` CMA `linux,cma-default`), and `qseecom@86600000` DT node in `msm8998-oneplus-common.dtsi`.
- **6.18 Kernel Work Items & Live `#45` Verification:**
  - [x] Verified `tz_mem` (`0x86200000`, `45 MiB`) + `qseecom_region` (`0xfdc00000..0xffbfffff`, `32 MiB` `< 4 GiB` CMA) probe without collisions (`qseecom 86600000.qseecom: initialized (qsee_version=0x1001000 whitelist=1 reentrancy=2)`).
  - [x] Verified `/dev/sg0`..`/dev/sg8` (`SG_IO` `INQUIRY` + `UFS_IOCTL_QUERY(GEOMETRY)` `rpmb_rw_size=0x40`) and `/dev/bsg/ufs-bsg0` exist and respond cleanly (`9/9`).
  - [x] Verified `/dev/qseecom` + `/dev/dma_heap/default_cma_region`: `QSEOS version = 0x14` (re-verified on `#51`), preloaded `keymaster` query (`app_id=65537`), and live TA load/unload of `ELF32` (`cmnlib` + `gfp5288` `id=131074`, `faceapp` `id=196610`) and `ELF64` (`cmnlib64` + `soter64` `id=262147`, `smplap64` `id=327684`).
  - [ ] Start stock `qseecomd` / Android Keymaster HAL end-to-end once Evolution-X Android 17 userspace boots (`firmware_class.path` includes `/vendor/firmware_mnt/image`; pair with clean-room `libQSEEComAPI` / thin `/dev/ion` shim in ROM).

### Track 2 — Goodix `GF5216`/`GF5288` Fingerprint (`gfp5288.mdt`) — **Kernel Complete (`#47`–`#52`)**
- **Commit:** `ae3159abd` (`drivers/input/misc/goodix_op5t.c`, `CONFIG_INPUT_GOODIX_OP5T=y`, `oneplus,fpdetect` + `goodix,fingerprint` DT nodes).
- **Verified Live (`#51` / `#52`):**
  - [x] `/dev/goodix_fp` (`10, 257`), `gf_input`, IRQ `57`, Netlink group `25`, `/sys/bus/platform/devices/*fingerprint*/sensor_version` -> `0x03 (gfp5288)`.
  - [x] `GF_IOC_INIT`, `ENABLE_POWER`, `RESET`, `ENABLE_IRQ`, `DISABLE_IRQ`, `CHIP_INFO`, `DISABLE_POWER` ioctls verified live on hardware (`#51`).
  - [ ] Wire Evolution X Biometrics AIDL HAL against `/dev/goodix_fp` + `gfp5288.mdt` once ROM boots.

### Track 3 — WCD9335 Codec Processing Engine (`cpe_9335.mdt`) — **Kernel Complete (`#46`–`#52`)**
- **Commits:** `24ea686cf`, `4b70a4d24`, `baad70753`, `503f6bb7d` (`sound/soc/codecs/wcd9335-cpe.c`, `wcd9335.c`, `CONFIG_SND_SOC_WCD9335_CPE=y`).
- **Verified Live (`#51` / `#52`):**
  - [x] Single-port `WCD9335_CPE_SS_MEM_BANK_0` ELF segment download, Tasha v2 internal calibrated RCO + 9.6 MHz MCLK gate, regmap SIDO supply, and async deferred load until codec probe (`WCD9335 CPE booted (cpe_9335.mdt): cmi_ver=1 sfr=0x4c690/80B shmem=0x56580 ss=0x05`, `running=1`, `errs=0`).
  - [x] Idempotent `cpe_load` sysfs toggle (`0 -> 1` clean restart) and `0 [O5T]` ADSP `SLIMBUS_0_RX Audio Mixer MultiMedia1` 9600-frame PCM playback verified live (`#51`).

### Track 4 & Remaining Non-Ported / Hardware-Gated Stock Items
- **Track 4 (`venus` Codec `f66691c0f`):** Full non-secure `venus` encoder (`/dev/video7`) and decoder (`/dev/video6`) verified live across `H264`, `HEVC`, `VP8`, `MPEG4`, `H263`, and `1080p` `H264`/`HEVC` with clean `last=1` EOS and runtime PM suspend/resume (`IS_V3` synthetic zero-buffer EOS `FBD`/`ETB`, `parse_raw_formats()` heterogeneous `num_planes` cap fix, `H264_TRANSFORM_8X8`, and `vcodec_power_v3` `hwmode` sequencing). Full **Widevine L1 / HDCP** remains out of scope (`hdcp*.mdt` missing from stock firmware image, unlocked bootloader disables OEM keybox provisioning, and TZ XPU `secure_region` + secure SMMU CBs block unprotected HLOS buffers; **Widevine L3** works).
- **Dash Charge (`oneplus_dash` `9a094c20e`):** Clean-room `oneplus_dash.c` (`4-0026`, `STM8S003F3U6TR`) verifies extracted `oneplus/dash_stm8s.bin` (`5746` bytes, `ver 0x37`) in the `~1850ms` post-reset bootloader window and exposes `/dev/dash` + `power_supply/oneplus-dash`.
- **TZ-Protected & Other-SKU Stock Items:** `nxp,p61` SPI eSE (`nxp_p61_ese.c` `b61b2af01` implemented, but `&blsp1_spi1` `GPIOs 0..3` are TZ XPU protected in `<0 4>` on production firmware so `&blsp1_spi1` stays `disabled`; `nfc0` PN553 I2C handles standard NFC), plus other-SKU stubs (`fpc*`, `cpe_9340`, `a530_zap`, `a630_gmu`, `slpi_v1`, `isdbtmm`, `keypad-backlight`).
