#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="/home/zero/oneplus5t-android17/backups"
CRIT_DIR="$BASE_DIR/critical_unique"

echo "=== Verifying SHA-256 Checksums Before Restore ==="
cd "$BASE_DIR"
grep "critical_unique/" SHA256SUMS.txt | sha256sum -c -

echo "=== Verifying ADB Root in Recovery ==="
adb devices -l
adb shell id | grep -q "uid=0(root)"

RESTORE_PARTS=(
    persist modemst1 modemst2 fsg fsc md5
    oem_dycnvbk oem_stanvbk param config
)

for p in "${RESTORE_PARTS[@]}"; do
    img="$CRIT_DIR/${p}.img"
    echo "Restoring $p from $img ..."
    adb push "$img" "/tmp/${p}.img"
    adb shell "dd if=/tmp/${p}.img of=/dev/block/bootdevice/by-name/$p bs=1048576 conv=fsync && rm -f /tmp/${p}.img"
done

echo "=== Critical EFS & Calibration Restore Complete ==="
