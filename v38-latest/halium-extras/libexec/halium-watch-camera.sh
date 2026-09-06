#!/bin/sh
# ПЕРЕНЕСЕНО из параллельного (Ubuntu Touch) проекта на этом же
# устройстве, 2026-09-02 -- контейнер и vendor.camera-provider-2-6
# общие для обоих проектов, скрипт полностью на уровне контейнера, не
# завязан на Lomiri/Mir.
#
# vendor.camera-provider-2-6 crashes in a hard loop due to missing EFS
# multi-cam calibration data on this device (root cause documented, not
# fixable in software). Repeated stop/start cycling sometimes clears a
# stuck ICP subdev fd from a previous crashed instance, occasionally
# letting the next attempt succeed -- not reliably understood, best-effort
# mitigation only, not a guaranteed fix. Loop forever and re-kick on every
# fresh lxc-android-config.service active transition (it sometimes
# restarts mid-session, and a one-shot triggered-once unit never re-fires
# for that).
LAST_STATE=""
while true; do
    CUR_STATE=$(systemctl is-active lxc-android-config.service 2>/dev/null)
    if [ "$CUR_STATE" = "active" ] && [ "$LAST_STATE" != "active" ]; then
        sleep 20
        for i in $(seq 1 40); do
            lxc-attach -n android -- sh -c 'stop vendor.camera-provider-2-6; sleep 1; start vendor.camera-provider-2-6' 2>/dev/null || true
            sleep 1
            pid1=$(lxc-attach -n android -- pgrep -f "provider@2.6" 2>/dev/null | head -1)
            if [ -n "$pid1" ]; then
                sleep 2
                pid2=$(lxc-attach -n android -- pgrep -f "provider@2.6" 2>/dev/null | head -1)
                if [ -n "$pid2" ] && [ "$pid1" = "$pid2" ]; then
                    break
                fi
            fi
        done
    fi
    LAST_STATE="$CUR_STATE"
    sleep 3
done
