---
name: qcom-rea-cleanroom
description: >-
  Use this skill when analyzing Qualcomm MSM8998 / OnePlus 5T proprietary vendor
  binaries (RIL/QMI, IMS, SLPI Sensors, Camera modules, Audio) using REA (Ghidra
  backend) and ELF static dissectors to produce clean-room hardware protocol
  specifications for Android 17 AIDL HALs.
---

# Qualcomm MSM8998 Clean-Room Binary Analysis Skill (`rea`)

This skill defines the clean-room reverse-engineering methodology for extracting hardware facts, QMI wire formats, calibration registry layouts, DT/driver mappings, and V4L2 camera register sequences from stock OxygenOS 10.0.1 `/vendor` binaries and stock kernel `4.4.205` (`OnePlusOSS/android_kernel_oneplus_msm8998`) using `rea` (`GHIDRA_INSTALL_DIR=/opt/ghidra_12.1.4_PUBLIC`).

## Clean-Room Rules

1. **Extract Hardware Facts Only**: Document wire protocols (QMI service IDs, message IDs, TLV tags), I2C slave addresses, register init tables, power rail sequences, and `/mnt/vendor/persist` calibration struct layouts.
2. **Never Copy Proprietary Binary Code**: Write all Android 17 AIDL HALs and mainline `6.18` kernel drivers from scratch in clean-room C/C++20/Rust against documented specs.

## Environment Setup (`192.168.1.87` or Ghidra host)

```bash
export GHIDRA_INSTALL_DIR=/opt/ghidra_12.1.4_PUBLIC
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
export REA_ANALYSIS_PROVIDER=ghidra
export REA_OBJDUMP_COMMAND=/usr/bin/objdump
```

## Key Analysis Commands

1. **Batch Subsystem Analysis Script:**
   Run [run_phase2_rea_analysis.sh](./scripts/run_phase2_rea_analysis.sh) to inspect `qcrild`, `rild`, `libril-qcril-hook-oem.so`, `sensor_calibrate.so`, `sensors.ssc.so`, `libqmiservices.so`, and all camera sensor/actuator/EEPROM modules.
2. **Targeted Function Decompilation & Search:**
   ```bash
   rea decompile-all <binary.so> --output <out_dir> --provider ghidra --json
   rea search <binary.so> <pattern> --kind strings --provider ghidra --json
   ```

## Existing Decompiled & Stock Artifacts (Archived on `192.168.1.15:~/oneplus5t-android17/`)
- **Decompiled Images (`rea_outputs/`):** `abl_decomp` (900 funcs), `hyp_decomp` (651 funcs), `tz_decomp` (3,494 funcs), `dashd_decomp`, `stock_batchb_decomp`, `stock_drivers_decomp` (1,357 funcs), `stock_dsi_pll_decomp` (146 funcs), `stock_pil_icnss_decomp` (491 funcs), `stock_venus_decomp` (425 funcs), `venus_fw_decomp`.
- **Extracted Specs (`rea_analysis_out/`):** `deep_hardware_analysis.txt`, `qmi_and_camera_exact_specs.txt`, plus `audio_misc/`, `camera/`, `ril/`, `sensors/` JSONs.
- **Stock Kernel & DTB (`kernel/stock-oneplus-msm8998/` & `stock_analysis/`):** `4.4.205` OOS 10.0.0 source (`2103804389`), `stock_analysis/config/stock_defconfig`, `stock_analysis/dtb/stock_02.dts`, `stock_analysis/boot/kallsyms.txt`. **Check these before running duplicate Ghidra jobs!**
- **Hardware Schematics:** `Schematic Diagram.pdf` (39 pages, decrypted) & `LAYOUT.pdf` (page 16, 24, 30, 36 have panel/PMIC/GPIO/Dash STM8S pinouts).
- **Signed Firmware & Cert Inspection Rule:** Parse ELF32/64 headers + hash segment + X.509 `Attestation CA` OU tags (`01 SW_ID`, `02 HW_ID`, `04 OEM_ID`, `06 MODEL_ID`, `07 SHA256`, `03 DEBUG`) rather than naive raw `0x30 0x82` scans.
- **References:** [qmi_qrtr_architecture.md](./references/qmi_qrtr_architecture.md) and `docs/02_signed_firmware_and_security.md`.
