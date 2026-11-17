#!/usr/bin/env bash
set -euo pipefail

export GHIDRA_INSTALL_DIR=/opt/ghidra_12.1.4_PUBLIC
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
export REA_ANALYSIS_PROVIDER=ghidra
export REA_OBJDUMP_COMMAND=/usr/bin/objdump
export TERM=xterm

BASE="/home/zero/oneplus5t-android17"
VENDOR="$BASE/vendor-blobs/extracted"
FW="$BASE/firmware/extracted"
OUT="$BASE/rea_analysis_out"
mkdir -p "$OUT/ril" "$OUT/sensors" "$OUT/camera" "$OUT/audio_misc"

echo "=== [1] REA INSPECT & DECOMPILE ON SMALLER CRITICAL BINARIES ==="
# 1a. qcrild & rild & libril-qcril-hook-oem.so & libril-qc-radioconfig.so
for bin in \
    "$VENDOR/bin/hw/qcrild" \
    "$VENDOR/bin/hw/rild" \
    "$VENDOR/lib64/libril-qcril-hook-oem.so" \
    "$VENDOR/lib64/libril-qc-radioconfig.so" \
    "$VENDOR/lib64/libqcrildatactl.so" \
    "$VENDOR/lib64/libqmi_cci.so"; do
    if [ -f "$bin" ]; then
        bname=$(basename "$bin")
        echo "Running REA inspect on $bname ..."
        rea inspect "$bin" --provider ghidra --json > "$OUT/ril/${bname}_inspect.json" 2>/dev/null || true
    fi
done

# Decompile libril-qcril-hook-oem.so and sensor_calibrate.so with rea decompile-all
echo "Running REA decompile-all on libril-qcril-hook-oem.so ..."
rea decompile-all "$VENDOR/lib64/libril-qcril-hook-oem.so" --output "$OUT/ril/oem_hook_decomp" --provider ghidra --json > "$OUT/ril/oem_hook_decomp.json" 2>/dev/null || true

echo "Running REA decompile-all on sensor_calibrate.so ..."
rea decompile-all "$VENDOR/lib64/sensor_calibrate.so" --output "$OUT/sensors/sensor_calibrate_decomp" --provider ghidra --json > "$OUT/sensors/sensor_calibrate_decomp.json" 2>/dev/null || true

echo "Running REA inspect on sensors.ssc.so and libsensor1.so ..."
rea inspect "$VENDOR/lib64/sensors.ssc.so" --provider ghidra --json > "$OUT/sensors/sensors.ssc_inspect.json" 2>/dev/null || true
rea inspect "$VENDOR/lib64/libsensor1.so" --provider ghidra --json > "$OUT/sensors/libsensor1_inspect.json" 2>/dev/null || true

echo "=== [2] DEEP PYTHON ELF / QMI / MCFG / SENSOR / CAMERA EXTRACTION ==="
python3 - << 'PY' > "$OUT/deep_hardware_analysis.txt"
import os, glob, subprocess, re, struct, json

BASE = "/home/zero/oneplus5t-android17"
VENDOR = f"{BASE}/vendor-blobs/extracted"
FW = f"{BASE}/firmware/extracted"
OUT = f"{BASE}/rea_analysis_out"

def run_cmd(cmd):
    return subprocess.run(cmd, capture_output=True, text=True).stdout

print("=================================================================")
print("1. BASEBAND / MCFG CARRIER PROFILES & FIRMWARE INVENTORY")
print("=================================================================")
mbns = sorted(glob.glob(f"{FW}/modem/**/*.mbn", recursive=True))
print(f"Total .mbn files in modem partition: {len(mbns)}")
for m in mbns:
    rel = os.path.relpath(m, f"{FW}/modem")
    sz = os.path.getsize(m)
    print(f"  {rel} ({sz} bytes)")

ver_info = f"{FW}/modem/verinfo/ver_info.txt"
if os.path.exists(ver_info):
    print("\nModem ver_info.txt:")
    print(open(ver_info).read())

print("\n=================================================================")
print("2. QMI SERVICE IDS & SYMBOLS IN LIBQMISERVICES & LIBRIL-QC-HAL-QMI")
print("=================================================================")
qmi_syms = run_cmd(["nm", "-D", "--defined-only", f"{VENDOR}/lib64/libqmiservices.so"])
qmi_objs = []
for line in qmi_syms.splitlines():
    parts = line.split()
    if len(parts) >= 3 and "get_service_object" in parts[2]:
        qmi_objs.append(parts[2])
print(f"Discovered {len(qmi_objs)} QMI Service Object getters in libqmiservices.so:")
for s in sorted(qmi_objs):
    print(f"  {s}")

ims_syms = run_cmd(["nm", "-D", "--defined-only", f"{VENDOR}/lib64/lib-imscmservice.so"])
print("\nKey symbols in lib-imscmservice.so:")
for line in ims_syms.splitlines()[:40]:
    print(" ", line)

print("\nStrings in libril-qc-hal-qmi.so matching QMI / RIL / IMS / NV / OnePlus:")
ril_strs = run_cmd(["strings", "-n", "6", f"{VENDOR}/lib64/libril-qc-hal-qmi.so"]).splitlines()
patterns = [r"qcril_qmi_", r"QMI_RIL_", r"persist\.vendor\.radio", r"oem_hook", r"ims_", r"mcfg", r"dsds"]
for pat in patterns:
    matches = sorted(set(s for s in ril_strs if re.search(pat, s, re.I)))
    print(f"\n  Pattern '{pat}' ({len(matches)} unique matches, showing top 25):")
    for m in matches[:25]:
        print(f"    {m}")

print("\n=================================================================")
print("3. SLPI SENSORS & PERSIST CALIBRATION REGISTRY ANALYSIS")
print("=================================================================")
ssc_strs = run_cmd(["strings", "-n", "5", f"{VENDOR}/lib64/sensors.ssc.so"]).splitlines()
for s in sorted(set(x for x in ssc_strs if any(k in x.lower() for k in ("qmi", "sns_", "suid", "persist", "cal", "gyro", "accel", "prox", "light", "mag"))))[:60]:
    print("  [sensors.ssc.so]", s)

reg_dir = f"{FW}/persist/sensors/registry/registry"
if os.path.isdir(reg_dir):
    print(f"\nPersist Sensor Registry files ({reg_dir}):")
    for fn in sorted(os.listdir(reg_dir)):
        fp = os.path.join(reg_dir, fn)
        if os.path.isfile(fp):
            data = open(fp, "rb").read()
            printable = [m.group().decode('ascii', 'ignore') for m in re.finditer(rb'[\x20-\x7e]{4,}', data)]
            print(f"  {fn} ({len(data)}B): {printable[:10]}")

print("\n=================================================================")
print("4. CAMERA SUBSYSTEM (SENSORS, ACTUATORS, OIS, EEPROM, CHROMATIX)")
print("=================================================================")
cam_libs = sorted(
    glob.glob(f"{VENDOR}/lib/libmmcamera_*.so") +
    glob.glob(f"{VENDOR}/lib64/libmmcamera_*.so") +
    glob.glob(f"{VENDOR}/lib/camera/**/*.so", recursive=True) +
    glob.glob(f"{VENDOR}/lib64/camera/**/*.so", recursive=True)
)
print(f"Total vendor camera modules found: {len(cam_libs)}")
for cl in cam_libs:
    if any(k in os.path.basename(cl) for k in ("imx", "s5k", "ov", "actuator", "eeprom", "ois", "pdaf", "laser", "flash")):
        print(" ", os.path.relpath(cl, VENDOR), f"({os.path.getsize(cl)}B)")
        strs = run_cmd(["strings", "-n", "4", cl]).splitlines()
        interesting = [s for s in strs if any(w in s.lower() for w in ("imx", "dw9", "lc8", "bu6", "gt24", "cat24", "0x", "i2c", "slave", "chip_id", "dumpling", "cheeseburger"))]
        if interesting:
            print("    ->", interesting[:12])
PY
cat "$OUT/deep_hardware_analysis.txt"
