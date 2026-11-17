# Signed Firmware, TrustZone & Security Inventory

Verified firmware signature, TrustZone TA, bootchain safety, and Play Integrity analysis for **OnePlus 5T (`dumpling` / MSM8998)**.

---

## 1. Signature Identities

Qualcomm X.509 / ELF certificate fields extracted from stock firmware images:
- **`HW_ID`:** `300200002A7041DB`
- **`OEM_ID`:** `2A70` (OnePlus)
- **`MODEL_ID`:** `41DB` (`17801`)

Because the Qualcomm PBL/TZ root-of-trust fuses enforce OEM/QTI signatures on signed ELF subsystems and QSEE TrustZone applications, modified signed binaries cannot execute on production hardware. Clean-room work modifies Linux kernel drivers and userspace only.

---

## 2. Linux PAS / MSA Signed Firmware (All Active on `#26`)

Stock split `.mdt` + `.bXX` segments are squashed via `pil-squasher` into `/lib/firmware/qcom/msm8998/oneplus/` inside initramfs:

| Firmware | Subsystem | `#26` Mainline Status |
| :--- | :--- | :--- |
| `mba.mbn` + `modem.mbn` (+ `modem*.jsn`, `27` `mcfg_sw.mbn` profiles) | MPSS (`4080000.remoteproc`, `MPSS.AT.2.0.c4.7-00070`) | **Running** (`qcom_q6v5_mss`, QRTR QMI + `rmtfs` + `tqftpserv`) |
| `adsp.mbn` (+ `adspr.jsn`, `adspua.jsn`) | ADSP (`remoteproc1`) | **Running** (`qcom_q6v5_pas`, `pd-mapper` `a38e66a35`) |
| `slpi.mbn` (`slpi_v1`/`slpi_v2` + `slpir.jsn`, `slpius.jsn`) | SLPI (`remoteproc0`) | **Running** (`qcom_q6v5_pas`, `pd-mapper`, SMP2P bit `12`) |
| `venus.mbn` (`venus-4.2`) | Venus Video Codec | **Running** (`venus_core`/`venus_dec`/`venus_enc` -> `/dev/video{0,1}`) |
| `ipa_fws.mbn` | IPA v3.1 Offload | **Running** (`ipa` -> `rmnet_ipa0`) |
| `a540_zap.mbn` | Adreno 540 ZAP Shader | **Running** (`msm_dpu` + `5000000.gpu` `/dev/dri/{card0,renderD128}`) |
| `wlanmdsp.mbn` (+ `ath10k/WCN3990/hw1.0/wlanmdsp.mbn`) | WCN3990 Wi-Fi MSA | **Running** (`ath10k_snoc` -> `wlan0` + NV MAC via QMI) |

---

## 3. Unsigned / Unauthenticated Firmware & Data Files

These files carry no QTI/OEM ELF RSA signature lock (or use public/board data tables):

| File | Size | Purpose | `#26` / Port Notes |
| :--- | :--- | :--- | :--- |
| `qca/crbtfw21.tlv` & `crnv21.bin` | `121K` / `4.4K` | WCN3990 Bluetooth patch & NV | **Working** (`hci0`) |
| `a530_pm4.fw`, `a530_pfp.fw`, `a540_gpmu.fw2` | `20K` / `17K` / `12K` | Adreno microcode (`qcom/`) | **Loaded & working** |
| `ath10k/WCN3990/hw1.0/board-2.bin` (`bdwlan*`) | `847K` | WCN3990 Wi-Fi BDF | **Loaded & working** |
| `cpe_9340.mbn` | `251K` | WCD9340 Xtensa CPE FW | Unsigned Xtensa FW; upstream `wcd934x` does not wire CPE wake-up |
| `tfa98xx.cnt` (stock `/vendor/etc/firmware/`) | `4.1K` | TFA9891 CoolFlux DSP tuning | Unauthenticated container; upstream `tfa989x.c` runs in **DSP bypass** without `.cnt` |
| `xbin/dashd` embedded MCU image | `73K` binary | Microchip `PIC16F` Dash FW | Uploaded over `/dev/dash`; proprietary binary |

---

## 4. Critical UFS Bootchain & Unique Calibration Partitions

### Bootchain (`backups/bootchain_fw/`) — NEVER Touch During Bring-Up
- `xbl`, `abl`, `tz`, `rpm`, `hyp`, `pmic`, `devcfg`, `apdp` (`8` bootchain partitions, backed up on `192.168.1.87`).
- A bad write or bricked bootchain risks falling back to Qualcomm **EDL (`05c6:9008`)**. Keep bring-up non-destructive via `fastboot boot` with the bootloader **unlocked** until both the ROM and recovery are production-stable (see Section 6).

### Device-Unique Radio & Sensor Calibration (`backups/critical_unique/`)
- `persist`, `modemst1`, `modemst2`, `fsg`, `oem_dycnvbk`, `oem_stanvbk`, `param` (backed up read-only `0444`, SHA-256 verified in `backups/SHA256SUMS.txt` on `192.168.1.87`).
- Never wipe `persist` or `modemst*`/`fsg` (`SNS_REG2_SVC` sensor calibration `sns.reg`/`als_rgb_cal_parameter`, `libdrmfs.so` `/mnt/vendor/persist/data/`, and IMEI/NV calibration depend on them).

---

## 5. QSEE TrustZone TAs & Clean-Room Recovery Plan (Linux `6.18` + AOSP 17)

### Why Mainline `6.18` Does Not Support QSEE TAs Out-of-the-Box
Upstream Linux `6.18` contains two Qualcomm TEE mechanisms, neither of which runs MSM8998 QSEE TAs as-is:
1. **`drivers/tee/qcomtee/` (`CONFIG_QCOMTEE`):** Probes `QCOM_SCM_SVC_SMCINVOKE` (`0x06` / `0x02`) via `qcom_scm_qtee_init()`. Verified live on `#26` (`172.16.42.1:2323`): `/sys/bus/platform/devices/firmware:scm` probed, and **no `qcomtee` device registered** (MSM8998 TZ returns `-EIO`; `stock_02.dts` also lacks `smcinvoke` and stock `qseecom.c` falls back on `-EIO`).
2. **`drivers/firmware/qcom/qcom_qseecom.c` (`CONFIG_QCOM_QSEECOM`):** Currently unset in `oneplus5t_android17_defconfig`. Even if enabled, `qcom_scm_qseecom_allowlist[]` in `qcom_scm.c` restricts probing to Snapdragon laptop boards (excluding `oneplus,dumpling`), and the driver only binds preloaded kernel clients (`uefisecapp`). It provides **no `/dev/qseecom` chardev**, **no dynamic `.mdt`/`.bXX` TA loading** into `qseecom_region` (`0x86600000`, `0x2200000` / CMA `0x1400000`), and **no re-entrant TZ listener callbacks** (`qcom_scm.c` hits `WARN_ON(res->result == QSEECOM_RESULT_BLOCKED_ON_LISTENER)` and carries a `TODO`).

### Feasibility & Subsystem Outcomes (`6.18` + AOSP 17)

| Component | Stock TZ / Blob (`OOS 10.0.1`) | Mainline `6.18` (`#52`) & Android 17 Status |
| :--- | :--- | :--- |
| **Goodix `GF5288` Fingerprint** | `gfp5288.mdt` / `gfp5288d.mdt` (`APP_ID=0x111`) + `goodix.fingerprint.msm8998.so` (`64-bit`) | **Kernel Port Done (`#52` live):** Clean-room `/dev/qseecom` (`d7f11f9cd`), `UFS_IOCTL_QUERY` + `/dev/sg0..8` RPMB (`3deb9bd3d`), and `goodix_op5t` + `oneplus,fpdetect` (`ae3159abd`: `/dev/goodix_fp`, `0x03 (gfp5288)`, Netlink `25`, ioctls + `gfp5288.mdt` QSEE load verified live). Only the AOSP 17 Biometrics AIDL HAL wrapper remains in Evolution X (`evolution-cnb`). |
| **Keymaster / KeyMint & Gatekeeper** | `keymaster.img` + `storsec.img` (`keymaster@3.0`, `gatekeeper@1.0`, `64-bit`) | **Kernel Port Done (`#52` live):** `/dev/qseecom` (`qsee_version=0x1001000`, `qseos_version=0x14`, preloaded `keymaster` `app_id=65537` query, `cmnlib`/`cmnlib64`/`soter64` TA load) + `/dev/sg*` + `CONFIG_SCSI_UFS_BSG=y` are active in kernel `#52`. Software KeyMint/Gatekeeper or QSEE AIDL wrappers can be wired in ROM. |
| **Widevine DRM** | `widevine.mdt` + `liboemcrypto.so` + `libcpion.so` (`drm@1.2`) | **Widevine L3 (SW)** works cleanly now. **Widevine L1 remains hardware-blocked** — `hdcp*.mdt` firmware images are missing from stock partitions, unlocked bootloader blocks keybox provisioning, and `liboemcrypto.so` also needs TZ XPU-protected `secure_region` (`0x5c00000`), SMMU `qcom,secure-context-bank` (`venus` + `gfx3d_secure`), and CPZ display handoff. |
| **Play Integrity / Play Protect** | `VerifiedBootDxe` ROT + TZ attestation | **`MEETS_BASIC_INTEGRITY`** achievable (`user` hardened build + GSF Android ID registration at `/android/uncertified/`). **`MEETS_DEVICE_INTEGRITY` and `MEETS_STRONG_INTEGRITY` cannot pass legitimately**, even with `/dev/qseecom` + relocked bootloader (`YELLOW` SelfSigned ROT, never OEM `GREEN`, and MSM8998 lacks StrongBox). |

### Engineering Plan to Recover QSEE TAs (`Keymaster`, `Gatekeeper`, `Goodix GF5216`)

Execute this as a dedicated post-Batch-B/UI-boot milestone (it is **not** a blocker to initial AOSP 17 bring-up):

1. **Kernel `6.18`: Minimal `/dev/qseecom` Port (`stock-oneplus-msm8998/drivers/misc/qseecom.c` -> `qcom_scm`):**
   - Replace removed downstream `scm_call2()` with `qcom_scm_call()`, `<asm/cacheflush.h>` `dmac_flush_range()` with DMA cache sync (`dma_sync_single_for_device()`), and drop `<linux/msm-bus.h>`, `<soc/qcom/subsystem_restart.h>`, and `<crypto/ice.h>` hooks (mainline `ufshcd` ICE `1db0000.crypto` manages storage crypto independently).
   - Replace `<linux/msm_ion.h>` (`ion_import_dma_buf` / `ion_phys`) with `dma_buf` FD attachment against a contiguous CMA `qseecom_region` pool (`size = 0x1400000`, backed by `CONFIG_DMABUF_HEAPS_CMA=y`).
   - Implement `APP_LOOKUP`, `APP_START` (loading `.mdt` + `.bXX` ELF segments), `QSEECOM_IOCTL_SEND_MODFD_CMD_64_REQ`, and re-entrant `QSEECOM_RESULT_BLOCKED_ON_LISTENER` (`2`) handling (`QSEECOM_IOCTL_REGISTER_LISTENER_REQ` / `RECEIVE_REQ` / `SEND_RESP_REQ`) for `RPMB_SERVICE` (`0x2000`), `/mnt/vendor/persist/data/` (`libdrmfs.so`), and time (`libdrmtime.so`).

2. **Kernel `6.18` + Userspace: Standard UFS RPMB Bridge (`CONFIG_SCSI_UFS_BSG`):**
   - Stock `vendor-blobs/extracted/lib64/librpmb.so` opens `/dev/sg%d` (`CONFIG_CHR_DEV_SG`) and issues downstream UFS query ioctls (`Error requesting ufs rpmb unit description via query ioctl`) that were removed upstream.
   - Enable `CONFIG_SCSI_UFS_BSG=y` in `6.18` (`/dev/bsg/ufs-bsg0`) and pair `qseecomd` with a clean-room UFS BSG RPMB transport shim (or small kernel compat bridge) so `storsec.img`, `keymaster`, `gatekeeper`, and `gfp5288` can authenticate RPMB frames.

3. **Kernel `6.18`: Goodix `GF5216` Platform Driver (`stock/drivers/input/fingerprint/goodix/`):**
   - Verified in `gf_spi.h`, `gf_spi.c`, and `platform.c`: `USE_PLATFORM_BUS = 1` (`AP_CONTROL_CLK` undefined). HLOS **never runs SPI transactions**; it only manages `fp-gpio-irq` (`tlmm 121`), `fp-gpio-reset` (`tlmm 75`), `fp-gpio-enable` (`tlmm 65`), `/dev/goodix_fp` ioctls, `/dev/uinput` nav events, and netlink `NETLINK_TEST = 25`.
   - Port the 4 tiny source files (`gf_spi.c`, `gf_spi.h`, `platform.c`, `netlink.c`) to `6.18` (`gpiod_*`, `wakeup_source`, DRM panel notifier instead of legacy `fb_notifier`).

4. **Android 17 Userspace: `libion` Shim + Clean-Room AIDL Wrappers over `64-bit` Vendor Blobs:**
   - **ION -> DMA-BUF shim:** `libQSEEComAPI.so` and `libgf_ca.so` open `/dev/ion`; provide an Android `libion` wrapper backed by `/dev/dma_heap/cma` and `/dev/dma_heap/system`.
   - **HIDL (`VNDK 29`) -> AIDL bridge:** All OOS `10.0.1` security/fingerprint binaries (`keymaster@3.0`, `gatekeeper@1.0`, `biometrics.fingerprint@2.1`, `goodix.fingerprint.msm8998.so`, `libgf_hal.so`, `libgf_ca.so`, `libQSEEComAPI.so`) are **`ELF 64-bit aarch64`** (unaffected by 32-bit deprecation), but AOSP 17 drops legacy HIDL FCM entries (`target-level="3"`, `libhidltransport`). Build **Stable AIDL** HAL services (`IFingerprint`, and optionally `IKeyMintDevice` / `IGatekeeper`) in AOSP 17 wrapping those `64-bit` QSEE libraries rather than resurrecting deprecated HIDL system dependencies.

---

## 6. End-of-Development Bootloader Relock Procedure (`YELLOW` Verified Boot v1.0)

> **CRITICAL RULE:** **Keep the bootloader UNLOCKED throughout all Linux `6.18` kernel and AOSP 17 development.** Never run `fastboot oem lock` until the final ROM, kernel, `dm-verity`, and recovery images are 100% tested and frozen.

### Verified Bootchain Behavior (`xbl.img` `VerifiedBootDxe.dll` + `abl.img` `LinuxLoader.efi`)
Binary inspection of `backups/bootchain_fw/{xbl,abl}.img` and `backups/fastboot_getvar_all.txt` confirms:
- **Verified Boot v1.0 (NOT AVB 2.0):** `dumpling` has **no `avb_custom_key` partition**. `VerifiedBootDxe.dll` (`xbl.img` @ `0xa2ca8`) verifies the **X.509 `AndroidBootSignature` (`boot_signer`)** appended to `boot.img` and `recovery.img`:
  - **OEM RSA key (`vb_verify_hash_oem_certificate`) -> `GREEN`** (normal stock boot).
  - **Self-signed X.509 cert (`vb_verify_hash_embedded_key`) -> `YELLOW`**: `abl.img` (`0x5c4c.c`) sets `androidboot.verifiedbootstate=yellow`, prints `"Your device has loaded a different operating system. Wait for 5 seconds before proceeding"` + `GetCertFingerPrint`, sends `SendROT` to Keymaster, and **boots while locked**.
  - **Missing or invalid signature when locked -> `RED`**: prints `"Your device is corrupt. It can't be trusted and will not boot."` and shuts down after 30 seconds.

### Risks While Locked
1. **`fastboot boot` RAM testing is disabled:** `abl.img` (`0x2dc94.c`) rejects it with `"boot is not allowed in Lock State"` / `"Fastboot boot command is not available in locked device"`.
2. **`fastboot flash` is blocked & unlock wipes data:** Recovering from a bad update requires unlocking (which factory-resets `userdata`); if **both** flashed `boot` and `recovery` enter `RED`, recovery requires **EDL (`05c6:9008`)**.
3. **`dm-verity` check:** `abl.img` (`0x5c4c.c` / `0x1dd28.c`) checks `veritymode`; misconfigured verity triggers a 30-second `"The dm-verity is not started in restart mode"` stall before booting.
4. **No Play Integrity / Widevine L1 gain:** Relocking yields `YELLOW` (never OEM `GREEN`) and does not grant `MEETS_DEVICE_INTEGRITY`, `MEETS_STRONG_INTEGRITY`, or Widevine L1. Its value is physical anti-tamper protection (`boot`/`recovery` RSA verification + blocking unsigned `fastboot flash`/`boot` without wiping `userdata`).

### Final Relock Checklist (Stable Production ROM Release Only)
1. **Verify image size & `dm-verity` while unlocked:** Confirm final signed `Image.gz-dtb` + ramdisk + appended signature stay inside `abl`'s `EndOfKernelRegion = 0x85600000` (`86 MiB`) ceiling, and ensure `dm-verity` (`enforcing` on signed `system`/`vendor`, or clean verified target flags without `restart` fallback) boots cleanly with no `abl` verity warning.
2. **Generate release signing key:** Create a dedicated 2048-bit RSA keypair + self-signed X.509 certificate and store the private key safely offline (every future OTA `boot.img` / `recovery.img` build must be signed with this key).
3. **Sign both partitions with matching AVB 1.0 target names (`boot_signer`):** `VerifiedBootDxe.dll` enforces exact partition target-name matching (`VB: vb_get_image_hash: name mismatch!`):
   - Sign `boot.img` with target path `/boot`.
   - Sign custom recovery (`recovery.img`) with target path `/recovery`.
4. **Flash and verify BOTH while still unlocked:**
   - `fastboot flash boot boot-signed.img`
   - `fastboot flash recovery recovery-signed.img`
   - Boot-test **both** normal Android (`boot`) and recovery (`recovery`) to 100% stability while still unlocked.
5. **Relock (`userdata` wipe):** Run `fastboot oem lock`, confirm the `YELLOW` 5-second certificate-fingerprint screen boots directly into stable Android 17, and re-verify recovery boot.
