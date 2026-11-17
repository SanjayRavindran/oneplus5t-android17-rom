#!/vendor/bin/sh
# OnePlus 5T (dumpling / MSM8998) Mainline 6.18 Hardware Bring-Up Script

LOG_DIR="/data/vendor/tombstones"
LOG_FILE="$LOG_DIR/hw_init.log"
mkdir -p "$LOG_DIR/pstore" 2>/dev/null || true

log_hw() {
    echo "[$(cat /proc/uptime 2>/dev/null | cut -d" " -f1)] $*" >> "$LOG_FILE" 2>/dev/null || true
    echo "oneplus5t-hw-init: $*" > /dev/kmsg 2>/dev/null || true
}

if [ -s "$LOG_FILE" ]; then
    cp -f "$LOG_FILE" "$LOG_DIR/hw_init_prev.log" 2>/dev/null || true
    sync
fi
: > "$LOG_FILE" 2>/dev/null || true
log_hw "Starting OnePlus 5T mainline 6.18 hardware bring-up"

# Capture any prior ramoops / pstore logs right away for crash diagnostics
for p in /sys/fs/pstore/*; do
    [ -e "$p" ] || continue
    cp -f "$p" "$LOG_DIR/pstore/$(basename "$p")" 2>/dev/null || true
    log_hw "Captured pstore file: $(basename "$p")"
done
sync

# Hold wake_lock so system_suspend does not put device into s2idle while USB/UI are active
if [ -w /sys/power/wake_lock ]; then
    echo DumplingStayAwake > /sys/power/wake_lock 2>/dev/null || true
fi

# Keep AMOLED brightness conservative (5% = 51/1023) & ensure system ownership
for bl in /sys/class/backlight/*; do
    [ -d "$bl" ] || continue
    chown system system "$bl/brightness" "$bl/max_brightness" 2>/dev/null || true
    chown system graphics "$bl/bl_power" 2>/dev/null || true
    chmod 0664 "$bl/bl_power" 2>/dev/null || true
    chmod 0664 "$bl/brightness" 2>/dev/null || true
    echo 51 > "$bl/brightness" 2>/dev/null || true
done

# Create /dev/block/by-name and /dev/disk/by-partlabel symlinks pointing to /dev/block/$DEVNAME.
# Never overwrite /dev/block/bootdevice/by-name (ueventd manages /dev/block/platform/soc@0/1da4000.ufshc/by-name/*).
mkdir -p /dev/block/by-name /dev/disk/by-partlabel
for uevent in /sys/class/block/sd*/uevent; do
    [ -f "$uevent" ] || continue
    DEVNAME=$(grep "^DEVNAME=" "$uevent" | cut -d= -f2)
    PARTNAME=$(grep "^PARTNAME=" "$uevent" | cut -d= -f2)
    if [ -n "$DEVNAME" ] && [ -n "$PARTNAME" ] && [ -e "/dev/block/$DEVNAME" ]; then
        ln -sfn "/dev/block/$DEVNAME" "/dev/block/by-name/$PARTNAME"
        ln -sfn "/dev/block/$DEVNAME" "/dev/disk/by-partlabel/$PARTNAME"
    fi
done
log_hw "Created /dev/block/by-name symlinks"

# Ensure tqftpserv readwrite directory exists & seed read-only mpss rfs calibration if absent
mkdir -p /data/vendor/tmp/tqftpserv 2>/dev/null || true
chmod 0700 /data/vendor/tmp/tqftpserv 2>/dev/null || true
if [ ! -f /data/vendor/tmp/tqftpserv/readwrite_ready ] && [ -d /mnt/vendor/persist/rfs/msm/mpss ]; then
    cp -r /mnt/vendor/persist/rfs/msm/mpss/* /data/vendor/tmp/tqftpserv/ 2>/dev/null || true
    touch /data/vendor/tmp/tqftpserv/readwrite_ready 2>/dev/null || true
    log_hw "Populated /data/vendor/tmp/tqftpserv from /mnt/vendor/persist/rfs/msm/mpss"
fi

# Wait up to 5s for rmtfs, tqftpserv, and sns-reg2-qmi to be active on QRTR before starting remoteprocs
for i in 1 2 3 4 5 6 7 8 9 10; do
    if pidof rmtfs >/dev/null 2>&1 && pidof tqftpserv >/dev/null 2>&1 && pidof sns-reg2-qmi >/dev/null 2>&1; then
        break
    fi
    sleep 0.5
done
sleep 2
log_hw "QRTR daemons: rmtfs=$(pidof rmtfs || echo none) tqftpserv=$(pidof tqftpserv || echo none) sns-reg2-qmi=$(pidof sns-reg2-qmi || echo none)"
sync

# Load Qualcomm Remoteproc, Wi-Fi, and Venus modules in order (ipa.ko omitted; not needed for Wi-Fi)
for mod in qcom-rng.ko qcom_q6v5_pas.ko qcom_q6v5_mss.ko ath.ko ath10k_core.ko ath10k_snoc.ko videobuf2-dma-contig.ko v4l2-mem2mem.ko venus-core.ko venus-dec.ko venus-enc.ko; do
    insmod "/vendor/lib/modules/$mod" >> "$LOG_FILE" 2>&1
    RC=$?
    log_hw "insmod $mod rc=$RC"
    sync
done

# Enable SSR recovery BEFORE starting any remoteprocs so subsystem faults do not reset the SoC
for r in /sys/class/remoteproc/remoteproc*; do
    [ -d "$r" ] || continue
    [ -w "$r/recovery" ] && echo enabled > "$r/recovery" 2>/dev/null || true
done

# Start SLPI & ADSP first, then MPSS one-by-one with logging & sync
for target in slpi adsp modem; do
    for r in /sys/class/remoteproc/remoteproc*; do
        [ -d "$r" ] || continue
        RNAME=$(cat "$r/name" 2>/dev/null || echo unknown); RFW=$(cat "$r/firmware" 2>/dev/null || echo none)
        case "$RNAME $RFW" in
            *"$target"*)
                RSTATE=$(cat "$r/state" 2>/dev/null || echo unknown)
                if [ "$RSTATE" = "offline" ]; then
                    log_hw "Starting $(basename "$r") ($RNAME)..."
                    sync
                    echo start > "$r/state" 2>> "$LOG_FILE"
                    RC=$?
                    sleep 1
                    RSTATE_AFTER=$(cat "$r/state" 2>/dev/null || echo unknown)
                    log_hw "Result $(basename "$r") ($RNAME): rc=$RC state=$RSTATE_AFTER"
                    sync
                fi
                ;;
        esac
    done
done

# Start any remaining offline remoteprocs (e.g. venus if represented)
for r in /sys/class/remoteproc/remoteproc*; do
    [ -d "$r" ] || continue
    RNAME=$(cat "$r/name" 2>/dev/null || echo unknown); RFW=$(cat "$r/firmware" 2>/dev/null || echo none)
    RSTATE=$(cat "$r/state" 2>/dev/null || echo unknown)
    if [ "$RSTATE" = "offline" ]; then
        log_hw "Starting remaining $(basename "$r") ($RNAME)..."
        sync
        echo start > "$r/state" 2>> "$LOG_FILE"
        sleep 1
        log_hw "Result $(basename "$r") ($RNAME): state=$(cat "$r/state" 2>/dev/null)"
        sync
    fi
done

# Wait up to 15s for wlan0 to register via QMI WLFW after MPSS starts
for i in $(seq 1 30); do
    [ -d /sys/class/net/wlan0 ] && break
    sleep 0.5
done

# Load WCD9335 CPE Xtensa DSP firmware
for c in /sys/bus/slimbus/devices/*/cpe_load; do
    [ -w "$c" ] || continue
    echo 1 > "$c" 2>/dev/null || true
done

# Wait up to 10s for ALSA Card 0 (O5T) control node after ADSP start
for i in $(seq 1 20); do
    [ -e /dev/snd/controlC0 ] && break
    sleep 0.5
done

# Configure ALSA Card 0 (O5T):
#   MM1 (CARD_0_DEV_0): TFA9890 Speaker (QUAT_MI2S_RX)
#   MM2 (CARD_0_DEV_1): WCD9335 Dual Mic Capture (SLIMBUS_0_TX ADC2/ADC3)
#   MM3 (CARD_0_DEV_2): WCD9335 Wired Headphones (SLIMBUS_0_RX AIF1_PB -> RX1/RX2 -> HPHL/HPHR)
#   MM4 (CARD_0_DEV_3): WCD9335 Earpiece (SLIMBUS_1_RX AIF2_PB -> RX0 -> RX INT0 -> EAR PA)
if [ -x /system/bin/tinymix ] && [ -e /dev/snd/controlC0 ]; then
    # Speaker (MultiMedia1)
    /system/bin/tinymix -D 0 "QUAT_MI2S_RX Audio Mixer MultiMedia1" 1 2>/dev/null || true
    /system/bin/tinymix -D 0 "Speaker Switch" 1 2>/dev/null || true

    # Wired Headphones (MultiMedia3 -> SLIMBUS_0_RX -> RX1/RX2 -> HPHL/HPHR)
    /system/bin/tinymix -D 0 "SLIMBUS_0_RX Audio Mixer MultiMedia3" 1 2>/dev/null || true
    /system/bin/tinymix -D 0 "SLIM RX1 MUX" "AIF1_PB" 2>/dev/null || true
    /system/bin/tinymix -D 0 "SLIM RX2 MUX" "AIF1_PB" 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX INT1_1 MIX1 INP0" "RX1" 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX INT2_1 MIX1 INP0" "RX2" 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX INT1 DEM MUX" "CLSH_DSM_OUT" 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX INT2 DEM MUX" "CLSH_DSM_OUT" 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX1 Digital Volume" 84 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX2 Digital Volume" 84 2>/dev/null || true
    /system/bin/tinymix -D 0 "HPHL Volume" 20 2>/dev/null || true
    /system/bin/tinymix -D 0 "HPHR Volume" 20 2>/dev/null || true
    /system/bin/tinymix -D 0 "COMP1 Switch" 1 2>/dev/null || true
    /system/bin/tinymix -D 0 "COMP2 Switch" 1 2>/dev/null || true

    # Earpiece (MultiMedia4 -> SLIMBUS_1_RX -> RX0 -> RX INT0 -> EAR PA)
    /system/bin/tinymix -D 0 "SLIMBUS_1_RX Audio Mixer MultiMedia4" 1 2>/dev/null || true
    /system/bin/tinymix -D 0 "SLIM RX0 MUX" "AIF2_PB" 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX INT0_1 MIX1 INP0" "RX0" 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX INT0 DEM MUX" "CLSH_DSM_OUT" 2>/dev/null || true
    /system/bin/tinymix -D 0 "RX0 Digital Volume" 84 2>/dev/null || true
    /system/bin/tinymix -D 0 "EAR PA Volume" 2 2>/dev/null || true

    # Mic Capture (MultiMedia2 <- SLIMBUS_0_TX <- ADC2/ADC3)
    /system/bin/tinymix -D 0 "MultiMedia2 Mixer SLIMBUS_0_TX" 1 2>/dev/null || true
    /system/bin/tinymix -D 0 "AIF1_CAP Mixer SLIM TX0" 1 2>/dev/null || true
    /system/bin/tinymix -D 0 "AIF1_CAP Mixer SLIM TX1" 1 2>/dev/null || true
    /system/bin/tinymix -D 0 "SLIM TX0 MUX" "DEC0" 2>/dev/null || true
    /system/bin/tinymix -D 0 "SLIM TX1 MUX" "DEC1" 2>/dev/null || true
    /system/bin/tinymix -D 0 "ADC MUX0" "AMIC" 2>/dev/null || true
    /system/bin/tinymix -D 0 "ADC MUX1" "AMIC" 2>/dev/null || true
    /system/bin/tinymix -D 0 "AMIC MUX0" "ADC2" 2>/dev/null || true
    /system/bin/tinymix -D 0 "AMIC MUX1" "ADC3" 2>/dev/null || true
    /system/bin/tinymix -D 0 "ADC2 Volume" 12 2>/dev/null || true
    /system/bin/tinymix -D 0 "ADC3 Volume" 12 2>/dev/null || true
    log_hw "Configured ALSA Card 0 O5T (Spk+Earpiece+HPH+Mic)"
fi

# Provision factory WLAN MAC address onto wlan0
if [ -d /sys/class/net/wlan0 ]; then
    WMAC=""
    if [ -s /mnt/vendor/persist/wlan_mac.bin ]; then
        WMAC=$(sed -n "s/^Intf0MacAddress=\([0-9A-Fa-f]\{12\}\).*/\1/p" /mnt/vendor/persist/wlan_mac.bin | head -n 1 | sed "s/../&:/g;s/:$//")
    fi
    if [ -z "$WMAC" ] && [ -x /vendor/bin/wlan-mac-qmi ]; then
        WMAC=$(/vendor/bin/wlan-mac-qmi 2>/dev/null | grep -E "^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$" | head -n 1)
    fi
    if [ -n "$WMAC" ]; then
        /system/bin/ip link set dev wlan0 down >> "$LOG_FILE" 2>&1 || true
        /system/bin/ip link set dev wlan0 address "$WMAC" >> "$LOG_FILE" 2>&1 || true
        /system/bin/ip link set dev wlan0 up >> "$LOG_FILE" 2>&1 || true
        log_hw "Set wlan0 MAC=$WMAC (now $(cat /sys/class/net/wlan0/address 2>/dev/null))"
    fi
fi

# Unblock Bluetooth & Wi-Fi rfkill soft blocks
for rf in /sys/class/rfkill/rfkill*/soft; do
    [ -w "$rf" ] && echo 0 > "$rf" 2>/dev/null || true
done

# Configure Bluetooth hci0 factory BD_ADDR so it leaves HCI_UNCONFIGURED
if [ -x /vendor/bin/oneplus5t-bt-init ]; then
    /vendor/bin/oneplus5t-bt-init >> "$LOG_FILE" 2>&1 || true
fi

if [ -x /vendor/bin/qrtr-lookup ]; then
    /vendor/bin/qrtr-lookup > "$LOG_DIR/qrtr_lookup.txt" 2>&1 || true
fi

setprop vendor.oneplus5t.hw_init_done 1
log_hw "Hardware init completed cleanly"
sync
