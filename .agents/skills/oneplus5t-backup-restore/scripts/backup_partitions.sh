#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="/home/zero/oneplus5t-android17/backups"
CRIT_DIR="$BASE_DIR/critical_unique"
BOOT_DIR="$BASE_DIR/bootchain_fw"
OS_DIR="$BASE_DIR/os_and_firmware"

mkdir -p "$CRIT_DIR" "$BOOT_DIR" "$OS_DIR"

echo "=== Verifying ADB in Recovery ==="
adb devices -l
adb shell id

echo "=== Saving full block device layout and GPT tables ==="
adb shell "cat /proc/partitions; ls -la /dev/block/bootdevice/by-name" > "$BASE_DIR/recovery_partitions.txt"

for disk in sda sdb sdc sdd sde sdf; do
    echo "Backing up GPT header of /dev/block/$disk ..."
    adb exec-out "dd if=/dev/block/$disk bs=4096 count=64 2>/dev/null" > "$CRIT_DIR/gpt_${disk}_head.bin"
done

CRITICAL_PARTS=(
    persist modemst1 modemst2 fsg fsc md5
    oem_dycnvbk oem_stanvbk param config
    cdt ddr devinfo sec limits dpo dip sti ssd
    frp keystore misc reserve reserve1 reserve2 reserve3
)

BOOTCHAIN_PARTS=(
    xbl xblbak rpm rpmbak tz tzbak hyp hypbak pmic pmicbak
    abl ablbak keymaster keymasterbak cmnlib cmnlibbak cmnlib64 cmnlib64bak
    devcfg storsec storsecbak mdtp mdtpsecapp mdtpsecappbak
    apdp msadp LOGO splash toolsfv logfs
    fw_4j1ed fw_8v1ee fw_4g9n4 fw_4t0n8
)

OS_FIRMWARE_PARTS=(
    modem dsp bluetooth boot boot_aging recovery vendor
)

dump_part() {
    local part="$1"
    local outdir="$2"
    local outfile="$outdir/${part}.img"
    echo -n "Dumping $part -> $outfile ... "
    adb exec-out "dd if=/dev/block/bootdevice/by-name/$part bs=1048576 2>/dev/null" > "$outfile"
    local sz
    sz=$(stat -c%s "$outfile")
    echo "OK (${sz} bytes)"
}

echo "=== Tier 1: Dumping Critical Unique & Calibration Partitions ==="
for p in "${CRITICAL_PARTS[@]}"; do
    dump_part "$p" "$CRIT_DIR"
done

echo "=== Tier 2: Dumping Bootchain, TrustZone & Panel Firmware Partitions ==="
for p in "${BOOTCHAIN_PARTS[@]}"; do
    dump_part "$p" "$BOOT_DIR"
done

echo "=== Tier 3: Dumping Baseband, DSP, BT, Boot, Recovery & Vendor Partitions ==="
for p in "${OS_FIRMWARE_PARTS[@]}"; do
    dump_part "$p" "$OS_DIR"
done

echo "=== Generating SHA-256 Manifest ==="
cd "$BASE_DIR"
sha256sum critical_unique/*.img critical_unique/*.bin bootchain_fw/*.img os_and_firmware/*.img > "$BASE_DIR/SHA256SUMS.txt"
echo "=== Backup Complete! Summary: ==="
du -sh "$CRIT_DIR" "$BOOT_DIR" "$OS_DIR"
wc -l "$BASE_DIR/SHA256SUMS.txt"
