#!/bin/sh
# CLAUDE_DEBUG 2026-09-05 (в37): пропатченная libcameraservice.so
# (исключение для hybris-клиента в handleAppOpMode -- без неё AppOps
# отзывает доступ к камере сразу после первого кадра, см. ФИКСЫ_ДЛЯ_
# В32.txt п.6.7) была застейджена как bind-mount через ${R} в
# mount-patched-v3.sh, но живьём подтверждено (ФИКСЫ_ДЛЯ_В36.txt
# п.15.4), что ${R} в этот момент указывает на пустой skeleton-путь,
# а не на реальный /system контейнера -- bind-mount тихо не применяется.
#
# У контейнера ДВА разных представления /system/lib64/...: хостовый
# путь (/var/lib/lxc/android/rootfs/...) и то, что реально видит
# процесс ВНУТРИ контейнера -- это не одно и то же. Единственный
# подтверждённый живьём рабочий способ -- писать ИЗНУТРИ контейнера
# через lxc-attach, а не с хоста.
#
# Файл читаем С ХОСТА через redirection (< ...) ДО lxc-attach -- так
# не важно, виден ли /opt/halium-lxc-bridge изнутри контейнера вообще:
# хостовый шелл открывает файл, lxc-attach просто наследует готовый fd
# на stdin.
#
# CLAUDE_DEBUG 2026-09-06 (в38): раньше запускался при старте phosh
# (After=phosh.service) с собственным wait-циклом, чтобы не добавлять
# лишний binder/файловый трафик в окно гонки phoc/surfaceflinger за
# композитор. Теперь триггерится on-demand из halium-camera-ondemand-
# refresh.sh -- к этому моменту рабочий стол уже точно поднят
# пользователем (он только что открыл камеру), ждать нечего, тот же
# аргумент, что и у halium-camera-provider-direct-start.sh.
lxc-attach -n android -- sh -c 'cat > /system/lib64/libcameraservice.so; chmod 644 /system/lib64/libcameraservice.so' \
    < /opt/halium-lxc-bridge/camera-fixes/libcameraservice.so.lib64 2>/dev/null || true

if [ -f /opt/halium-lxc-bridge/camera-fixes/libcameraservice.so.lib32 ]; then
    lxc-attach -n android -- sh -c 'if [ -d /system/lib ]; then cat > /system/lib/libcameraservice.so; chmod 644 /system/lib/libcameraservice.so; fi' \
        < /opt/halium-lxc-bridge/camera-fixes/libcameraservice.so.lib32 2>/dev/null || true
fi
