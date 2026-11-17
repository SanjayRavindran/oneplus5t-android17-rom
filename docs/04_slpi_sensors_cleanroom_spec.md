# Clean-Room Specification: SLPI Sensors Subsystem (`SLPI.HB.2.0` / `libsensor1.so` / `sensors.ssc.so`)

## 1. Key Architectural Finding (`REA` Decompilation of `libsensor1.so` & `sensor_calibrate.so`)

Using `rea decompile-all` on `/vendor/lib64/libsensor1.so` and `/vendor/lib64/sensor_calibrate.so`, we established a critical architectural distinction for the OnePlus 5T (`MSM8998`):
- **Firmware:** `SLPI.HB.2.0-00169-M8998AZL-1` (`slpi.mdt` running on the Hexagon Sensor Low Power Island DSP).
- **Wire Protocol:** Unlike SDM845+ devices (which use `SNS_CLIENT_SVC` Protobuf-over-QMI SEE), `SLPI.HB.2.0` on MSM8998 exposes the **Qualcomm Sensor1 (`SNS_SMGR`, `SNS_REG2`, `SNS_SAM_*`) QMI IDL services** directly over QMI/QRTR (`AF_QIPCRTR`).

---

## 2. Complete SLPI QMI Service ID Table (`libsensor1.so` `.dynsym` & `svc_map`)

Extracted directly from `qmi_idl_service_object` structures in `libsensor1.so`:

| QMI Service ID | Decimal | IDL Symbol Name | IDL Version | Max Msg Bytes | Subsystem Role |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`0x100`** | `256` | `SNS_SMGR_SVC_qmi_idl_service_object_v01` | `v1` | `1676` | **Core Sensor Manager** ( enumerate, streaming report rate, selftest/calibration) |
| **`0x104`** | `260` | `SNS_SAM_AMD_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Absolute Motion Detector |
| **`0x105`** | `261` | `SNS_SAM_RMD_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Relative Motion Detector |
| **`0x106`** | `262` | `SNS_SAM_VMD_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Vehicle Motion Detector |
| **`0x108`** | `264` | `SNS_DIAG_DSPS_SVC_qmi_idl_service_object_v01` | `v1` | `196` | DSPS Diagnostics |
| **`0x10a`** | `266` | `SNS_SAM_BTE_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Basic Tap / Bring-to-Ear |
| **`0x10d`** | `269` | `SNS_SMGR_INTERNAL_SVC_qmi_idl_service_object_v01/v02` | `v1/v2` | `2068` | Internal SMGR events |
| **`0x10f`** | `271` | `SNS_REG2_SVC_qmi_idl_service_object_v02` | `v2` | `275` | **Sensor Registry Service** (reads/writes `/mnt/vendor/persist/sensors/sns.reg`) |
| **`0x110`** | `272` | `SNS_SAM_MAG_CAL_SVC_qmi_idl_service_object_v01` | `v1` | `44` | Magnetometer Calibration |
| **`0x111`** | `273` | `SNS_SAM_FILTERED_MAG_SVC_qmi_idl_service_object_v01` | `v1` | `1493` | Filtered Magnetometer |
| **`0x112`** | `274` | `SNS_SAM_ROTATION_VECTOR_SVC_qmi_idl_service_object_v01` | `v1` | `1803` | Android Rotation Vector fusion |
| **`0x113`** | `275` | `SNS_SAM_QUATERNION_SVC_qmi_idl_service_object_v01` | `v1` | `1787` | Gyro Quaternion fusion |
| **`0x114`** | `276` | `SNS_SAM_GRAVITY_VECTOR_SVC_qmi_idl_service_object_v01` | `v1` | `1790` | Gravity + Linear Acceleration fusion |
| **`0x115`** | `277` | `SNS_SAM_SENSOR_THRESH_SVC_qmi_idl_service_object_v01` | `v1` | `812` | Threshold-gated sensor reporting (Proximity / ALS) |
| **`0x117`** | `279` | `SNS_SAM_ORIENTATION_SVC_qmi_idl_service_object_v01` | `v1` | `1913` | 9-axis Orientation fusion |
| **`0x118`** | `280` | `SNS_TIME2_SVC_qmi_idl_service_object_v02` | `v2` | `48` | AP-to-SLPI timestamp synchronization |
| **`0x119`** | `281` | `SNS_SAM_BASIC_GESTURES_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Basic gestures |
| **`0x11a`** | `282` | `SNS_SAM_TAP_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Tap detection |
| **`0x11b`** | `283` | `SNS_SAM_FACING_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Device facing up/down |
| **`0x11f`** | `287` | `SNS_OEM_1_SVC_qmi_idl_service_object_v01` | `v1` | `1523` | OnePlus OEM gesture/pocket sensor |
| **`0x123`** | `291` | `SNS_SAM_GYROINT_SVC_qmi_idl_service_object_v01` | `v1` | `691` | Gyro integration for EIS (`oneplus5T_vstab_db`) |
| **`0x125`** | `293` | `SNS_SAM_PED_SVC_qmi_idl_service_object_v01` | `v1` | `1776` | Step Counter & Step Detector (Pedometer) |
| **`0x128`** | `296` | `SNS_SAM_SMD_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Significant Motion Detector |
| **`0x129`** | `297` | `SNS_SAM_CMC_SVC_qmi_idl_service_object_v02` | `v2` | `1980` | Coarse Motion Classifier |
| **`0x12b`** | `299` | `SNS_SAM_GAME_ROTATION_VECTOR_SVC_qmi_idl_service_object_v01` | `v1` | `1803` | 6-axis Game Rotation Vector |
| **`0x12e`** | `302` | `SNS_SAM_TILT_DETECTOR_SVC_qmi_idl_service_object_v01` | `v1` | `412` | Wake-up Tilt Detector |
| **`0x13a`** | `314` | `SNS_SAM_DPC_SVC_qmi_idl_service_object_v02` | `v2` | `86` | Device Position Classifier |
| **`0x13e`** | `318` | `SNS_SAM_DEV_ORI_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Screen Auto-Rotation Device Orientation |
| **`0x140`** | `320` | `SNS_SAM_QMAG_CAL_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Magnetometer Calibration |
| **`0x141`** | `321` | `SNS_SAM_GYRO_CAL_SVC_qmi_idl_service_object_v01` | `v1` | `86` | Gyroscope Calibration |

---

## 3. Hardware Sensor IDs (`SNS_SMGR_ID_*`) & Calibration Flow

Decompilation of `calibrate()` (`0x102014`) and `ssc_calibrate::req_sensor_cal()` (`0x10278c`) in `sensor_calibrate.so` reveals the exact `SNS_SMGR` hardware sensor IDs and calibration request structure:

| Hardware Sensor | `SensorID` (`uint8_t`) | `DataType` (`uint8_t`) | Calibration File / Storage |
| :--- | :--- | :--- | :--- |
| **Accelerometer (BMI160 / LSM6DSM)** | `10` (`0x0A`) | `0` (`PRIMARY`) | `/mnt/vendor/persist/sensors/sns.reg` |
| **Gyroscope (BMI160 / LSM6DSM)** | `20` (`0x14`) | `0` (`PRIMARY`) | `/mnt/vendor/persist/sensors/sns.reg` + `/vendor/etc/GyroOffset_db.config` |
| **Magnetometer (AK09915)** | `30` (`0x1E`) | `0` (`PRIMARY`) | `/mnt/vendor/persist/sensors/sns.reg` |
| **Proximity (TMD3725)** | `40` (`0x28`) | `0` (`PRIMARY`) | `/mnt/vendor/persist/sensors/sns.reg` |
| **Ambient Light / RGB (TMD3725)** | `40` (`0x28`) | `1` (`SECONDARY`) | `/mnt/vendor/persist/sensors/als_rgb_cal_parameter` |

### `SNS_SMGR_SINGLE_SENSOR_TEST_REQ_V01` (`msg_id = 0x000D`) Wire Layout
- `svc_num`: `0` (`SNS_SMGR_SVC` = QMI Service `0x100`)
- `msg_id`: `0x000D` (`13`), payload length `12` bytes (`0x0C`):
  - `offset +0x00` (`uint8_t`): `SensorID` (`10`, `20`, or `40`)
  - `offset +0x01` (`uint8_t`): `DataType` (`0` = Primary, `1` = Secondary)
  - `offset +0x04` (`uint32_t`): `TestType` (`0` = Self-test / Zero-bias calibration)
  - `offset +0x08` (`uint8_t[4]`): `SaveToRegistry_valid = 1`, `SaveToRegistry = 1`, `ApplyCalNow_valid = 1`, `ApplyCalNow = 1` (`0x01010101`)

---

## 4. Clean-Room Android 17 `ISensors` AIDL HAL Design

To bring up all sensors cleanly on Android 17 with a mainline Linux kernel:
1. **SLPI Remoteproc Bring-Up:** Mainline `qcom_q6v5_pas` boots `slpi.mdt` (`SLPI.HB.2.0`), which registers QMI services `0x100`–`0x142` on the kernel QRTR bus (`AF_QIPCRTR`).
2. **Registry Daemon / Handler:** Our clean-room `ISensors` AIDL service implements the `SNS_REG2_SVC` (`0x10f`) responder/client backed by `/mnt/vendor/persist/sensors/sns.reg` so factory accelerometer, gyroscope, and proximity calibration offsets are loaded into SLPI at boot.
3. **Sensor Discovery & Streaming:**
   - Send `SNS_SMGR_ALL_SENSOR_INFO_REQ_V01` (`msg_id = 0x05`) and `SNS_SMGR_SINGLE_SENSOR_INFO_REQ_V01` (`msg_id = 0x06`) to QMI service `0x100` to enumerate all physical sensors and ranges.
   - Map `batch()` and `activate()` calls from Android 17 `SensorService` to `SNS_SMGR_BUFFERING_REQ_V01` (`msg_id = 0x21`) and `SNS_SAM_*_ENABLE_REQ_V01` messages over `AF_QIPCRTR`.
   - Convert incoming `SNS_SMGR_BUFFERING_IND_V01` (`msg_id = 0x22`) Q16 fixed-point samples (`val * (1.0f / 65536.0f)`) into Android 17 `aidl::android::hardware::sensors::Event` structs and post them to the Event FMQ (Fast Message Queue).
