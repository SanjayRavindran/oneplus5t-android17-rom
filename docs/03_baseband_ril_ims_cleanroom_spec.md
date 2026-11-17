# Clean-Room Specification: Baseband, QMI RIL & IMS Architecture (`MSM8998` / `dumpling`)

## 1. Baseband & Subsystem Firmware Matrix

Extracted from `/vendor/firmware_mnt/verinfo/ver_info.txt` on the OnePlus 5T (`ONEPLUS A5010_43_201029`):

| Subsystem | Firmware Image Build ID | Binary Files (`modem/image/`) |
| :--- | :--- | :--- |
| **Meta Build** | `MSM8998.LA.1.1.c6-00017-STD.PROD-1.271073.1.277412.1` | Timestamp: `2020-10-29 21:14:55` |
| **Modem (`MPSS`)** | `MPSS.AT.2.0.c4.7-00070-8998_GEN_PACK-2.271073.1.277412.1` | `mba.mbn` (`230 KB`), `modem.mdt` + `modem.b00`–`b30` |
| **Audio DSP (`ADSP`)** | `ADSP.HT.3.0-00387-CB8998-1` | `adsp.mdt` + `adsp.b00`–`b11`, `adspr.jsn`, `adspua.jsn` |
| **Sensor Island (`SLPI`)** | `SLPI.HB.2.0-00169-M8998AZL-1` | `slpi.mdt` + `slpi.b00`–`b14` |
| **Video Codec (`Venus`)** | `VIDEO.VE.4.4-00043-PROD-1` | `venus.mdt` + `venus.b00`–`b04` |
| **Audio Codec DSP (`WDSP`)** | `WDSP.9340.1.0.c1-00008-W9340AAAAAAAZQ-1` | `cpe_9340.mdt` + `cpe_9340.b00`–`b20` |
| **WLAN (`WCN3990`)** | `WLAN.HL.1.0.c6-00126-QCAHLSWMTPLZ-1.211883.1.278648.1` | `wlanmdsp.mbn` (`3.38 MB`), `bdwlan.bin`, `bdwlan.b*` |
| **Bluetooth (`WCN3990`)** | `BTFM.CHE.2.1.1-00300-QCACHROMZ-1` | `crbtfw21.tlv`, `crnv21.bin` (`bluetooth` partition) |
| **TrustZone (`QSEE`)** | `TZ.BF.4.0.6-00157-M8998AAAAANAZT-1.175038.2` | `tz.img` (`sde3`) |
| **RPM** | `RPM.BF.1.7-00128-M8998AAAAANAZR-1` | `rpm.img` (`sde1`) |

---

## 2. Carrier MCFG Profiles (`mcfg_sw.mbn`) & PDC Provisioning

Our Phase 2 extraction identified **27 carrier `mcfg_sw.mbn` bundles** inside `modem/image/modem_pr/mcfg/configs/mcfg_sw/generic/`:
- **APAC / India:** `airtel/volte`, `idea/commerci`, `reliance/commerci` (Jio VoLTE), `vodafone/volte/india`, `oem/oversea/commerci/mtnl_bsn`
- **North America:** `na/att/volte`, `na/tmo/commerci`
- **Europe:** `eu/ee/commerci`, `eu/elisa/commerci/fi`, `eu/h3g/commerci/{denmark,uk}`, `eu/telefoni/commerci/uk`, `eu/vodafone/volte/netherla`
- **China & SEA:** `china/{cmcc,ct,cu}`, `sea/{cht,fareasto,ytl}`
- **Global Fallback:** `common/row/commerci/mcfg_sw.mbn` (`64,468 bytes`) and `oem/oversea/commerci/mcfg_sw.mbn` (`39,108 bytes`)

### Clean-Room MCFG Activation Flow (`QMI_PDC`)
Analysis of `libril-qc-hal-qmi.so` confirms that carrier VoLTE/VoWiFi/CA policy loading uses the **Persistent Device Configuration (`QMI_PDC`)** service:
1. `QCRIL_EVT_QMI_RIL_PDC_LIST_CONFIGURATION` — Queries active/stored MCFG configurations on the MPSS EFS.
2. `QCRIL_EVT_QMI_RIL_PDC_DEACTIVATE_CONFIGURATION` — Deactivates previous slot configuration on SIM swap.
3. `QCRIL_EVT_QMI_RIL_PDC_LOAD_CONFIGURATION` — Streams chunks of the matched `mcfg_sw.mbn` (selected by SIM MCC/MNC or IIN prefix) over `QMI_PDC`.
4. `QCRIL_EVT_QMI_RIL_PDC_SELECT_CONFIGURATION` — Selects and activates the loaded `CONFIG_ID` per subscription (`SUB0` / `SUB1`).

---

## 3. Complete 39-Service QMI Catalog (`libqmiservices.so`)

Static symbol and `.data.rel.ro` analysis of `/vendor/lib64/libqmiservices.so` reveals all 39 QMI IDL service object accessors used by the stock telephony/radio stack:

- **Core Telephony & SIM:** `dms`, `nas`, `uim`, `uim_remote`, `uim_http`, `voice` (v02), `wms`, `pbm`, `cat` (v02), `at`
- **Data Plane & QoS:** `wds`, `wda`, `dfs`, `dpm`, `dsd`, `qos`, `ipa`, `ott`
- **IMS (VoLTE / VoWiFi / ViLTE / RCS):** `imss`, `imsa`, `imsdcm`, `imsp`, `imsrtp`, `imsvt`, `ims_qmi`, `imsprivate`, `atp`
- **RF, Thermal, SAR & Coexistence:** `rfrpe`, `sar`, `antswitch`, `coex`, `blm`, `lte`, `csvt`
- **System, Security & Storage:** `pdc`, `sfs`, `ssctl` (v02), `svs`, `auth`, `lowi`

---

## 4. Clean-Room IMS (`VoLTE` / `VoWiFi`) Architecture (`lib-imscmservice.so`)

Analysis of `lib-imscmservice.so` (`IMSCmDevice`) and `libril-qc-hal-qmi.so` establishes how IMS operates on MSM8998 without requiring Android 10 Java/HIDL blobs:

1. **Modem-Resident SIP/IMS Stack:** On `MPSS.AT.2.0.c4.7`, the SIP user agent, IMS registration state machine, and RoHC/IPsec security associations run **inside the Hexagon Modem DSP (`MPSS`)**, controlled via QMI services:
   - `imss` (`QMI_IMSS`): Configures VoLTE (`ENABLE_VOLTE`), VoWiFi (`ENABLE_VOWIFI`, `WIFI_CALL_PREFERENCE`), ViLTE (`ENABLE_VT`), and RTT/Presence toggles.
   - `imsdcm` (`QMI_IMSDCM`): Data Connection Manager handoff between LTE (`eDCM_RAT`) and WLAN (`ServiceAvailableWLAN`), plus PDP ID binding (`IMSDevGetPdpIDFromRAT`).
   - `imsa` (`QMI_IMSA`): Reports IMS registration status (`RegistrationStatus`, `GenericRegStatus`) and service availability (`ServiceAvailable`).
   - `imsrtp` (`QMI_IMSRTP`): Configures RTP/RTCP media ports and codecs with the ADSP for VoWiFi/ViLTE media streams, while standard VoLTE voice calls route through `QMI_VOICE` + QDSP6 `CSVS` voice paths.
2. **Android 17 Clean-Room Implementation:**
   - Our clean-room `IRadio` AIDL HAL exposes `android.hardware.radio.ims.IImsRadio` (and standard `IRadioNetwork` / `IRadioVoice` IMS capabilities) by translating Android 17 AIDL calls directly into `QMI_IMSS`, `QMI_IMSA`, and `QMI_VOICE` messages over `AF_QIPCRTR`.

---

## 5. Dual-SIM Dual-Standby (`DSDS`) & Data Plane (`qcom_ipa`)

1. **DSDS Binding:**
   - Each SIM slot (`slotId = 0`, `slotId = 1`) binds its `QMI_NAS`, `QMI_VOICE`, `QMI_WMS`, and `QMI_WDS` client handles using `QMI_UIM` subscription binding (`QMI_NAS_BIND_SUBSCRIPTION_REQ` / `QMI_WDS_BIND_SUBSCRIPTION_REQ`).
   - Default Data Subscription (`DDS`) switching uses `QMI_DSD` (`DsdSystemStatusMessage` / `QMI_DSD_BIND_SUBSCRIPTION_REQ`).
2. **Data Call Setup (`WDS` + `WDA` + `IPA`):**
   - Set data format via `QMI_WDA_SET_DATA_FORMAT_REQ` (`QMAP` / `MAPv4` multiplex header enabled, hardware accelerated path via `IPA`).
   - Bind multiplex ID (`mux_id = 1..n`) via `QMI_WDS_BIND_MUX_DATA_PORT_REQ` (endpoint type `HSUSB` / `EMBEDDED` IPA pipes).
   - Start network interface via `QMI_WDS_START_NETWORK_INTERFACE_REQ` (APN, IP family 4/6) and query IP/DNS/Gateway/MTU via `QMI_WDS_GET_RUNTIME_SETTINGS_REQ`.
   - Configure the Linux `rmnet_data0` interface created on top of upstream `ipa` netdev with the returned IPv4/IPv6 addresses and routes.
