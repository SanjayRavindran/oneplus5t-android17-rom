# QMI over QRTR (`AF_QIPCRTR`) Architecture on Mainline MSM8998

## 1. Downstream (Linux 4.4) vs. Mainline (Linux 6.12+) IPC Transport

| Layer | Stock OxygenOS 10.0.1 (Linux 4.4) | Mainline Linux + Android 17 |
| :--- | :--- | :--- |
| **Physical / Shared Memory** | `SMD` / `SMEM` / `GLINK` downstream drivers | Upstream `qcom_glink_smem` / `rpmsg` / `qcom_q6v5_mss` (`remoteproc`) |
| **IPC Router Socket** | Downstream `msm_ipc_router` (`AF_MSM_IPC` = 27) | Upstream Linux **QRTR (`AF_QIPCRTR` = 42)** (`net/qrtr/qrtr.c`) |
| **Modem File System** | Proprietary `rmt_storage` daemon | Open-source **`rmtfs`** daemon serving `modemst1`, `modemst2`, `fsg`, `fsc` over QRTR |
| **Data Plane (Packet I/O)** | Downstream `rmnet_ipa` + `ipacm` + `netmgrd` | Upstream **`qcom_ipa`** (`drivers/net/ipa/`) + `rmnet` netdev over MAPv4 |
| **Control Plane (RIL)** | Proprietary `qcrild` + `libril-qc-hal-qmi.so` | **Clean-Room Android 17 `IRadio` AIDL HAL** speaking QMI directly over `AF_QIPCRTR` |

## 2. Core QMI Services Exposed by `MPSS.AT.2.0.c4.7` on QRTR

| Service ID (Hex) | Name | Subsystem Role |
| :--- | :--- | :--- |
| `0x00` | `QMI_CTL` | Control service (client ID allocation on legacy QMUX; implicit per-socket on QRTR) |
| `0x01` | `QMI_WDS` | Wireless Data Service (PDP context activation, IPv4/IPv6 bearer, RMNET mux binding) |
| `0x02` | `QMI_DMS` | Device Management Service (IMEI, ESN, radio power mode, operating mode, baseband version) |
| `0x03` | `QMI_NAS` | Network Access Service (PLMN scan, signal strength, LTE/WCDMA/GSM registration, network selection) |
| `0x05` | `QMI_WMS` | Wireless Messaging Service (SMS PDU send/receive/ack, cell broadcast) |
| `0x07` | `QMI_AUTH` | Authentication Service |
| `0x09` | `QMI_VOICE` | Voice Service (MO/MT call dial, answer, hangup, conference, USSD, supplementary services) |
| `0x0B` | `QMI_UIM` | User Identity Module (SIM card status, PIN/PUK, APDU transmit, logical channels, DSDS slot routing) |
| `0x0E` | `QMI_RMTFS` | Remote File System (`rmtfs` EFS read/write/iovec handler for `modemst1`/`modemst2`/`fsg`) |
| `0x10` | `QMI_LOC` | Location / GNSS Service (PDS/LOC fixes, XTRA injection) |
| `0x12` | `QMI_IMSS` | IMS Settings Service (VoLTE, VoWiFi, ViLTE, SMS-over-IP config toggles) |
| `0x13` | `QMI_IMSA` | IMS Application Service (IMS registration status, PDP status, RTP statistics) |
| `0x24` | `QMI_PDC` | Persistent Device Configuration (loading/activating carrier `mcfg_sw.mbn` profiles) |
| `0x30` | `QMI_IPA` | IP Accelerator Control (`qcom_ipa` modem filter/route table setup) |
| `0x100`–`0x141` | `QMI_SNS` (`SNS_*`) | Legacy MSM8998 SLPI Sensor1 services (`0x100` `SNS_SMGR_SVC`, `0x103` `SNS_PM_SVC`, `0x10f` `SNS_REG2_SVC`, `0x107..0x132` `SNS_SAM_*`; newer SoCs use `0x190` SEE) |
