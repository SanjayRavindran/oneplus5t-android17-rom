# Evolution X 17.0 (`dumpling`) — Release & Flashing Guide

> [!WARNING]
> Flashing custom ROM and recovery images modifies UFS partitions and formats `/data` and `/metadata` (`cache`). Verify your backup of critical calibration partitions (`persist`, `modemst1`, `modemst2`, `fsg`, `param`, `oem_dycnvbk`, `oem_stanvbk`) before flashing. You assume all risk.

## 1. Release Artifacts (Staged on `.15` and `.87` under `~/oneplus5t-android17/release/`)

| File | Size | SHA-256 | Notes |
| --- | --- | --- | --- |
| `EvolutionX-17.0-20261116-dumpling-12.4-Unofficial.zip` | `2,674,767,810` B (`2.49 GiB`) | `d9afd47b731412e06e085d1526bd4e50e5b3d00f3a6b2662addcf1bdf7180763` | Full Evolution X 17.0 (`v12.4`, `userdebug`, `cp2a`) OTA package with bundled **GApps** (`TARGET_USES_MINI_GAPPS := true`) |
| `recovery.img` | `28,938,240` B (`27.60 MiB`) | `f255589bcd22f3aea8ab02e2b374dc5bd1876653da6f239c9eff858444304073` | Evolution X Recovery (`6.18.0-msm8998+ #58`, EROFS/F2FS/ADB sideload, fits `64 MiB` `recovery` partition) |
| `boot.img` | `15,241,216` B (`14.54 MiB`) | `b9072661064d2e553a69d17f58ba39d985f6c7e9af14f44fee13dac4e1646db` | Evolution X Android 17 boot image (`6.18.0-msm8998+ #58`, also included inside the `.zip`) |
| `system.img` | raw `2,943,541,832` B (`2,807.18 MiB`) | `ad91316b879e7e9c5ec9059e5f74425ab6226d0ab2e87bb2b2849143be078acf` | EROFS image (`system` + `system_ext` + `product`), fits stock `3,072 MiB` `sde21` |
| `vendor.img` | raw `213,917,772` B (`204.01 MiB`) | `f5128c3e1cbef30ecb6278b50bd65c8610ae870a8e9d59168853618f580bd3e6` | EROFS image, fits stock `1,024 MiB` `sdf6` |

## 2. Included GApps (`WITH_GMS=true`, `gms_mini.mk`)

GApps are **built directly into the ROM** (`system/product`, `system/system_ext`, `system`) — do **not** flash a separate GApps zip:
- Google Play Store (`Phonesky`), Play Services (`PrebuiltGmsCore` + Dynamite modules + Google Services Framework), Setup Wizard & Pixel Setup
- Google App (`Velvet`), Chrome + Android System WebView (`WebViewGoogle` / `TrichromeLibrary`), Gboard (`LatinIMEGooglePrebuilt`)
- Phone (`GoogleDialer`), Messages (`PrebuiltBugle`), Contacts (`GoogleContacts`), Calendar (`CalendarGooglePrebuilt`), Files (`FilesPrebuilt`), Clock, Calculator, Google TTS, Android Auto stub, Pixel Launcher (`NexusLauncherRelease`)

## 3. Flashing Instructions (from `.87` or any host with `fastboot` + `adb`)

### Prerequisites
- Bootloader **unlocked** (`unlocked:yes`).
- Stock OnePlus 5T **OxygenOS 10 (QKQ1.191014.012)** firmware/modem/tz/abl (`sde10` `modem`, `sde11` `dsp`, `sde24` `bluetooth` intact — do **not** wipe them).
- Battery >= 50%.
- Keep `~/oneplus5t-android17/backups/` safe. Do **not** relock the bootloader.

### Step 1 — Boot into Fastboot Mode
If not already in fastboot, hold **Volume Up + Power** (or run `adb reboot bootloader`), then verify connection:
```bash
cd ~/oneplus5t-android17/release
sha256sum -c EvolutionX-17.0-20261116-dumpling-12.4-Unofficial.zip.sha256sum images.sha256sum
fastboot devices
```

### Step 2 — Flash Evolution X Recovery (`recovery.img`)
*(Why Evolution X Recovery? TWRP 3.7.1 uses an older 4.4 kernel and lacks Android 17 EROFS / Linux 6.18 F2FS parity. Evolution X Recovery uses the `#55` Linux `6.18.0-msm8998+` kernel and matches `fstab.qcom`.)*
```bash
fastboot flash recovery recovery.img
```
Reboot directly into the newly flashed recovery using the hardware buttons (**Volume Down/Up → Recovery mode → Power**) or:
```bash
fastboot oem reboot-recovery
```
*(Or test without flashing first with `fastboot boot recovery.img` — already verified live on `5f5440e4` with USB `18d1:d001`.)*

### Step 3 — Format Data / Metadata (Required for Clean Install)
Because `dumpling` has no dedicated hardware `metadata` GPT partition, `fstab.qcom` uses the `256 MiB` `cache` partition (`sda3`) for `/metadata` and formats `/data` (`sda13`) as `f2fs` with `aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized` FBE.
1. In **Evolution X Recovery**, choose **Factory reset → Format data / factory reset** and confirm.
2. Wait for `/data` and `/metadata` formatting to finish, then return to the main menu.

### Step 4 — Sideload Evolution X ROM (Includes GApps + `boot.img`)
1. On the phone in **Evolution X Recovery**, select **Apply update → Apply from ADB**.
2. On the host (`.87`), run:
```bash
adb sideload ~/oneplus5t-android17/release/EvolutionX-17.0-20261116-dumpling-12.4-Unofficial.zip
```
3. Note: The OTA `updater-script` flashes `system` (`sde21`), `vendor` (`sdf6`), and `boot` (`sde19`) unconditionally and leaves `recovery`, `modem`, `dsp`, `bluetooth`, and `persist` untouched.
4. Once sideload reaches `100%` (even if host prints `Total xfer: 1.00x` or `47%` when complete), select **Reboot system now** on the phone.

### Optional Alternative — Direct `fastboot` Flash (from `.15`)
If preferred instead of `adb sideload`:
```bash
fastboot flash boot boot.img
fastboot flash recovery recovery.img
fastboot flash system system.img
fastboot flash vendor vendor.img
fastboot -w
fastboot reboot
```

## 4. Verification & Known Bring-Up Caveats
- **Tested live on `5f5440e4` (`6.18.0-msm8998+ #58`):**
  - Full Android 17 Evolution X `17.0` UI boots to `sys.boot_completed=1` with Mesa3D `freedreno` hardware GPU acceleration (`Adreno 540`, `0` GPU/IOMMU translation faults), `drm_hwcomposer`, `Synaptics s3508` touch, backlight (`ILights`), `spmi_haptics` Vibrator AIDL HAL, SLPI QMI `0x100` Sensors AIDL HAL (5 hardware + 8 fused sensors), `WCN3990` Wi-Fi (`ath10k_snoc`, 2.4 GHz / 5 GHz scan), `WCN3990` Bluetooth (`hci0` with `.bt_nv.bin` BD_ADDR), Audio Card 0 `O5T` (`TFA9890` Speaker / `WCD9335` Headphone / `AMIC2+3` capture via ALSA), Thermal AIDL 3, and Battery (`bq27411-0`).
- **Remaining clean-room boundaries (kernel hardware bound, Android UI HAL deferred):**
  - **Camera:** `camss` (`/dev/video0..5`), `imx398`/`imx376k`/`imx371`, `lc898217xc` VCMs, and `24c64` EEPROMs are bound in kernel; `libcamera` SoftISP + `ICameraProvider` AIDL HAL is not yet integrated into camera apps.
  - **Cellular / RIL:** `MPSS` (`mpss`) and QMI `UIM` (`0x00b`) are online, and mock `minradio.virtual` has been removed; full QMI `IRadio` + `ipa` mobile data is not yet active.
  - **NFC & Fingerprint:** `3-0028` `nxp-nci_i2c` (Linux `AF_NFC` netlink) lacks an Android `INfc` netlink HAL; `gf_input` `/dev/goodix_fp` remains on virtual FP stub (`QSEE` blobs incompatibility).
