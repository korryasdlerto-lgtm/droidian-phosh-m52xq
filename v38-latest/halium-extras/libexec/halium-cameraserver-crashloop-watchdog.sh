#!/bin/sh
# ПЕРЕНЕСЕНО из параллельного (Ubuntu Touch) проекта на этом же
# устройстве, 2026-09-02 -- уровень контейнера/logcat, не завязано на
# Lomiri/Mir.
#
# cameraserver commonly shows up as "multiple PIDs" in a plain `pgrep -x
# cameraserver` -- but that is USUALLY misleading: init kills and restarts
# it on every crash, and the old PID lingers as a harmless zombie (state Z)
# until reaped. This watchdog instead counts REAL crashes (a genuine
# "Fatal signal ... (cameraserver)" logcat line -- SIGABRT/SIGSEGV/etc,
# not zombie PID count) in a rolling window. Only if that rate is
# genuinely high (a true crash-loop) does it act -- and the action is to
# kick vendor.camera-provider-2-6, since a REAL cameraserver crash-loop is
# most likely caused by the same wedged/crashing vendor HAL underneath it.
LOG_TAG="halium-cameraserver-crashloop-watchdog"
WINDOW_SECONDS=60
CRASH_THRESHOLD=5
CHECK_INTERVAL=15
MIN_KICK_GAP=30
LAST_KICK_FILE=/run/halium-cameraserver-crashloop-watchdog.last-kick

log() {
    echo "$LOG_TAG: $1" | systemd-cat -t "$LOG_TAG" -p info
}

while true; do
    sleep "$CHECK_INTERVAL"

    since_ts=$(date -d "-${WINDOW_SECONDS} seconds" '+%m-%d %H:%M:%S')
    crash_count=$(lxc-attach -n android -- logcat -d -v time 2>/dev/null \
        | awk -v since="$since_ts" '$0 >= since' \
        | grep -c "Fatal signal.*(cameraserver)")

    [ "${crash_count:-0}" -lt "$CRASH_THRESHOLD" ] && continue

    now=$(cat /proc/uptime 2>/dev/null | cut -d. -f1)
    last=$(cat "$LAST_KICK_FILE" 2>/dev/null || echo 0)
    gap=$((now - last))
    if [ "$gap" -lt "$MIN_KICK_GAP" ]; then
        continue
    fi

    log "detected genuine cameraserver crash-loop ($crash_count real Fatal signal hits in ${WINDOW_SECONDS}s) -- kicking vendor.camera-provider-2-6"
    lxc-attach -n android -- sh -c 'stop vendor.camera-provider-2-6; sleep 1; start vendor.camera-provider-2-6' 2>/dev/null
    echo "$now" > "$LAST_KICK_FILE"
done
