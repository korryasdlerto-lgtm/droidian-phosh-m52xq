#!/bin/sh
# НАЙДЕНО 2026-09-03: изначальная идея (systemd .path, следит за
# /etc/localtime через inotify) не сработала -- inotify watch на
# путь-символическую ссылку следует за её ТЕКУЩЕЙ целью, а смена цели
# ссылки (то, что делает timedatectl set-timezone) не модифицирует ни
# старый, ни новый файл-цель, так что событие никогда не приходит.
# Подтверждено живьём: даже ручной `ln -sf` не будил .path-юнит.
#
# Вместо слежки за файлом -- подписка на реальный источник события:
# D-Bus-сигнал PropertiesChanged от самого systemd-timedated (это
# именно то, что дёргает Настройки при смене пояса). Это по-прежнему
# НЕ опрос -- gdbus monitor блокируется на чтении сокета и просыпается
# только когда реально приходит сигнал, никакого периодического будильника.
exec gdbus monitor --system --dest org.freedesktop.timedate1 --object-path /org/freedesktop/timedate1 2>/dev/null | \
while read -r _line; do
    case "$_line" in
        *Timezone*)
            /usr/libexec/halium-save-timezone.sh
            ;;
    esac
done
