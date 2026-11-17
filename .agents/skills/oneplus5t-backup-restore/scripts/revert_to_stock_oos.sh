#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="/home/zero/oneplus5t-android17/backups"
OS_DIR="$BASE_DIR/os_and_firmware"
TWRP_IMG="$BASE_DIR/twrp-3.7.1_12-2-cheeseburger_dumpling.img"
SPARSE_CHUNK="512M"

DRY_RUN=false
AUTO_YES=false
WITH_FIRMWARE=false
WITH_DATA=false
KEEP_DATA=false
NO_REBOOT=false

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Revert OnePlus 5T (dumpling) back to backed-up stock OxygenOS 10 from fastboot mode.

Options:
  --dry-run        Verify fastboot device, backup files, and SHA-256 checksums without flashing
  --yes            Skip interactive confirmation prompt
  --with-firmware  Also flash modem, dsp, bluetooth, boot_aging, and logdump images
  --with-data      Also RAM-boot TWRP after flashing and restore userdata_decrypted.tar.gz to /data
  --keep-data      Do not format userdata (NOT recommended when coming from Android 17 FBE/F2FS)
  --no-reboot      Leave device in fastboot/recovery instead of rebooting into OxygenOS at the end
  -h, --help       Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) DRY_RUN=true; shift ;;
        --yes) AUTO_YES=true; shift ;;
        --with-firmware) WITH_FIRMWARE=true; shift ;;
        --with-data) WITH_DATA=true; shift ;;
        --keep-data) KEEP_DATA=true; shift ;;
        --no-reboot) NO_REBOOT=true; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

if [[ "$WITH_DATA" == "true" && "$KEEP_DATA" == "true" ]]; then
    echo "ERROR: --with-data and --keep-data cannot be combined." >&2
    exit 1
fi

run_cmd() {
    if [[ "$DRY_RUN" == "true" ]]; then
        echo "[DRY-RUN] $*"
    else
        "$@"
    fi
}

echo "=== 1. Checking Backup Archive & SHA-256 Checksums ==="
cd "$BASE_DIR"
CORE_PARTS=(boot recovery vendor cache system)
CHECK_FILES=()
for p in "${CORE_PARTS[@]}"; do
    CHECK_FILES+=("os_and_firmware/${p}.img")
done
if [[ "$WITH_FIRMWARE" == "true" ]]; then
    for p in modem dsp bluetooth boot_aging logdump; do
        CHECK_FILES+=("os_and_firmware/${p}.img")
    done
fi
if [[ "$WITH_DATA" == "true" ]]; then
    CHECK_FILES+=("os_and_firmware/userdata_decrypted.tar.gz")
fi

for f in "${CHECK_FILES[@]}"; do
    [[ -f "$BASE_DIR/$f" ]] || { echo "ERROR: Missing backup file: $BASE_DIR/$f" >&2; exit 1; }
    grep " $f\$" "$BASE_DIR/SHA256SUMS.txt" | sha256sum -c -
done

echo "=== 2. Checking Fastboot Device ==="
FB_OUT=$(fastboot devices)
echo "$FB_OUT"
if [[ -z "$FB_OUT" ]]; then
    echo "ERROR: No fastboot device detected. Put phone in fastboot (Vol Up + Power) and retry." >&2
    exit 1
fi
if [[ "$WITH_DATA" == "true" && ! -f "$TWRP_IMG" ]]; then
    echo "ERROR: TWRP image missing for --with-data restore: $TWRP_IMG" >&2
    exit 1
fi

if [[ "$DRY_RUN" == "false" && "$AUTO_YES" == "false" ]]; then
    echo ""
    echo "WARNING: This will overwrite boot, recovery, vendor, cache, and system on the connected device"
    if [[ "$KEEP_DATA" == "false" ]]; then
        echo "         and FORMAT userdata (all current OS data will be erased)."
    fi
    read -rp "Type REVERT to continue: " confirm
    [[ "$confirm" == "REVERT" ]] || { echo "Aborted."; exit 1; }
fi

echo "=== 3. Flashing Stock Boot, Recovery, Vendor, Cache, System ==="
run_cmd fastboot flash boot "$OS_DIR/boot.img"
run_cmd fastboot flash recovery "$OS_DIR/recovery.img"
run_cmd fastboot flash cache "$OS_DIR/cache.img"
run_cmd fastboot -S "$SPARSE_CHUNK" flash vendor "$OS_DIR/vendor.img"
run_cmd fastboot -S "$SPARSE_CHUNK" flash system "$OS_DIR/system.img"

if [[ "$WITH_FIRMWARE" == "true" ]]; then
    echo "=== 4. Flashing Stock Modem / DSP / Bluetooth / Boot-Aging / Logdump ==="
    for p in modem dsp bluetooth boot_aging logdump; do
        run_cmd fastboot flash "$p" "$OS_DIR/${p}.img"
    done
fi

if [[ "$KEEP_DATA" == "false" ]]; then
    echo "=== 5. Formatting Userdata & Re-flashing Stock Cache ==="
    run_cmd fastboot -w
    run_cmd fastboot flash cache "$OS_DIR/cache.img"
fi

if [[ "$WITH_DATA" == "true" ]]; then
    echo "=== 6. RAM-Booting TWRP & Restoring Decrypted /data Archive ==="
    run_cmd fastboot boot "$TWRP_IMG"
    if [[ "$DRY_RUN" == "false" ]]; then
        for _ in $(seq 1 30); do
            if adb devices | grep -qE "recovery|device"; then
                break
            fi
            sleep 1
        done
        adb shell "id" | grep -q "uid=0(root)"
        echo "Streaming userdata_decrypted.tar.gz to /data ..."
        gzip -dc "$OS_DIR/userdata_decrypted.tar.gz" | adb exec-in "tar -xpf - -C /data"
        adb shell "sync"
        if [[ "$NO_REBOOT" == "false" ]]; then
            adb reboot
        fi
    else
        echo "[DRY-RUN] Wait for ADB root in TWRP, then extract $OS_DIR/userdata_decrypted.tar.gz into /data"
    fi
else
    if [[ "$NO_REBOOT" == "false" ]]; then
        echo "=== 6. Rebooting into Stock OxygenOS ==="
        run_cmd fastboot reboot
    fi
fi

echo "=== Stock OxygenOS Revert Complete ==="
