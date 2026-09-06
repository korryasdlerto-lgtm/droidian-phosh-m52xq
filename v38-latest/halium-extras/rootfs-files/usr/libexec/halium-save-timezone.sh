#!/bin/sh
# Вызывается systemd-юнитом halium-timezone-changed.path по событию
# изменения /etc/localtime (inotify), а не по таймеру -- пользователь
# 2026-09-03 справедливо указал, что держать лишний таймер в фоне ради
# редко меняющейся настройки не нужно. Логика та же, что раньше жила
# внутри halium-save-state.sh: /etc/timezone не надёжен (timedatectl
# его не обновляет), настоящий источник -- цель симлинка /etc/localtime.
readlink -f /etc/localtime 2>/dev/null | sed 's#.*/zoneinfo/##' > /userdata/saved-timezone.txt
