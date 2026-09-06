#!/bin/sh
# ПЕРЕНЕСЕНО из параллельного (Ubuntu Touch) проекта на этом же
# устройстве, 2026-09-02 -- уровень kernel audit/host journal, не
# завязано на Lomiri/Mir.
#
# The actual camera.provider@2.6 binary crashing (SIGABRT, missing EFS
# multi-cam calibration data -- the long-documented, not software-fixable
# root cause) does NOT auto-restart on its own, and a lone provider crash
# leaves the viewfinder permanently black until someone manually kicks
# the provider. The kernel's own audit subsystem reports this crash
# directly on the HOST journal as ANOM_ABEND (no lxc-attach/logcat needed) --
# comm= varies (the crashing thread's own name), but exe= always points at
# the provider binary, so that's the stable match key.
#
# Reacts to a SINGLE hit (unlike the crashloop-watchdog's 5-hit threshold)
# since a lone provider crash has already been confirmed to leave the
# viewfinder dead with no auto-recovery.
LOG_TAG="halium-camera-provider-abend-watchdog"
WINDOW_SECONDS=10
CHECK_INTERVAL=2
MIN_KICK_GAP=20
LAST_KICK_FILE=/run/halium-camera-provider-abend-watchdog.last-kick

log() {
    echo "$LOG_TAG: $1" | systemd-cat -t "$LOG_TAG" -p info
}

while true; do
    sleep "$CHECK_INTERVAL"

    since_ts=$(date -d "-${WINDOW_SECONDS} seconds" '+%Y-%m-%d %H:%M:%S')
    hit_count=$(journalctl -o short-precise --since "$since_ts" --no-pager 2>/dev/null \
        | grep -c "ANOM_ABEND.*exe=\"/vendor/bin/hw/android.hardware.camera.provider")

    [ "${hit_count:-0}" -eq 0 ] && continue

    now=$(cat /proc/uptime 2>/dev/null | cut -d. -f1)
    last=$(cat "$LAST_KICK_FILE" 2>/dev/null || echo 0)
    gap=$((now - last))
    if [ "$gap" -lt "$MIN_KICK_GAP" ]; then
        continue
    fi

    log "detected camera provider crash (ANOM_ABEND, $hit_count hit(s) in ${WINDOW_SECONDS}s) -- kicking vendor.camera-provider-2-6 (up to 40x, 1s apart, same recovery cycle as the other camera watchdogs)"
    for i in $(seq 1 40); do
        lxc-attach -n android -- sh -c 'stop vendor.camera-provider-2-6; sleep 1; start vendor.camera-provider-2-6' 2>/dev/null || true
        sleep 1
        pid1=$(lxc-attach -n android -- pgrep -f "provider@2.6" 2>/dev/null | head -1)
        if [ -n "$pid1" ]; then
            sleep 2
            pid2=$(lxc-attach -n android -- pgrep -f "provider@2.6" 2>/dev/null | head -1)
            if [ -n "$pid2" ] && [ "$pid1" = "$pid2" ]; then
                log "provider stable after $i attempt(s), stopping early"
                break
            fi
        fi
    done
    echo "$now" > "$LAST_KICK_FILE"
    log "kick sequence done, will re-check on next detection"
done
