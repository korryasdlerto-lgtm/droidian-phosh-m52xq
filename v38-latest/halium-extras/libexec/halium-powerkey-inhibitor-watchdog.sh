#!/bin/sh
# НАЙДЕНО 2026-09-01 живьём: короткое нажатие кнопки питания переставало
# блокировать/разблокировать экран -- logind ждёт реакции ВСЕХ holder'ов
# инхибитора "handle-power-key" (systemd-inhibit --list), но некоторые
# из них оказываются от уже МЁРТВЫХ процессов (COMM отображается как
# "n/a" -- gnome-session/gsd-компоненты, гибнущие вместе с рестартом
# phosh.service, но почему-то не освобождающие свой inhibitor fd).
# Живой фикс, который реально помог -- "systemctl restart phosh.service"
# (не убирает старые мёртвые локи из списка, но новый живой инстанс
# всё равно снова принимает нажатия -- вероятно, logind реально слушает
# именно САМУЮ СВЕЖУЮ активную сессию, а не блокируется на мёртвых
# записях; либо сам handler у phosh со временем "подвисает" независимо
# от инхибиторов, и рестарт лечит именно это). Этот вотчдог -- то же
# самое действие, но превентивно и по таймеру, вместо ожидания жалобы
# "кнопка не работает".
LOG_TAG="halium-powerkey-inhibitor-watchdog"
log() {
    echo "$LOG_TAG: $1" | systemd-cat -t "$LOG_TAG" -p info
}

# Формат "systemd-inhibit --list --no-legend": WHO UID USER PID COMM WHAT ...
# PID -- 4-е поле. Мёртвый holder узнаём по "kill -0 PID" (процесса нет).
DEAD_COUNT=$(systemd-inhibit --list --no-legend 2>/dev/null | grep 'handle-power-key' | \
    awk '{print $4}' | while IFS= read -r pid; do
        [ -n "$pid" ] && ! kill -0 "$pid" 2>/dev/null && echo dead
    done | wc -l)

if [ "${DEAD_COUNT:-0}" -ge 2 ] 2>/dev/null; then
    log "found $DEAD_COUNT dead handle-power-key inhibitor(s) -- restarting phosh.service"
    systemctl restart phosh.service
    log "phosh.service restarted"
fi

exit 0
