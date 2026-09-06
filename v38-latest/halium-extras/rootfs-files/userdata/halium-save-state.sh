#!/bin/sh
mkdir -p /userdata/nm-connections-backup /userdata/bluetooth-backup /userdata/polkit-rules-backup
cp -a /etc/NetworkManager/system-connections/. /userdata/nm-connections-backup/ 2>/dev/null
cp -a /var/lib/bluetooth/. /userdata/bluetooth-backup/ 2>/dev/null
cp /etc/default/locale /userdata/saved-locale.txt 2>/dev/null
cp -a /etc/polkit-1/rules.d/. /userdata/polkit-rules-backup/ 2>/dev/null
date -u +%s > /userdata/saved-clock.txt

# НАЙДЕНО 2026-09-06: часовой пояс нигде не сохранялся -- после вайпа
# /data или сброса на дефолт устройство остаётся на UTC/дефолтном
# поясе, пока пользователь не выставит его заново вручную. /etc/timezone
# содержит просто имя зоны текстом (например "Europe/Kyiv") -- этого
# достаточно для восстановления через timedatectl.
cat /etc/timezone > /userdata/saved-timezone.txt 2>/dev/null

# НАЙДЕНО 2026-09-02: этот скрипт вызывается таймером каждые 2 минуты
# БЕЗУСЛОВНО (halium-save-state.timer) и раньше писал сырое текущее
# значение /sys/class/backlight/.../brightness в файл без всякой
# проверки. Если тик таймера попадал ровно на момент анимации
# затемнения экрана перед блокировкой/сном (gsd-power плавно снижает
# яркость до минимума за несколько секунд до фактического
# выключения), в файл улетала эта транзитная почти-нулевая яркость --
# а не реальная, которую выставил пользователь. При следующем
# восстановлении (halium-restore-state.sh на загрузке) применялось
# именно это заниженное значение вместо настоящего.
# Фикс: сохранять яркость только когда сессия НЕ idle (logind
# выставляет IdleHint=yes именно во время dim-to-sleep перехода) --
# тогда снимок гарантированно делается в момент, когда пользователь
# реально использует экран с реальной яркостью.
_seat0_session=$(loginctl list-sessions --no-legend 2>/dev/null | awk '$4=="seat0"{print $1; exit}')
_idle="no"
if [ -n "$_seat0_session" ]; then
    _idle=$(loginctl show-session "$_seat0_session" -p IdleHint --value 2>/dev/null)
fi
if [ "$_idle" != "yes" ]; then
    cat /sys/class/backlight/panel0-backlight/brightness 2>/dev/null > /userdata/saved-brightness.txt
fi

# НАЙДЕНО 2026-07-25: "pactl get-sink-volume/get-sink-mute @DEFAULT_SINK@"
# на этой сборке pactl 14.2 падает с "No valid command specified" --
# похоже, эти подкоманды тут не работают (причина не выяснялась, не
# стоило времени). Рабочая альтернатива -- распарсить блок нужного
# синка из "pactl list sinks" (это же confirmed рабочим много раз за
# сессию). LC_ALL=C ОБЯЗАТЕЛЕН -- иначе на русской локали pactl выводит
# "Аудиоприёмник по умолчанию:" вместо "Default Sink:", и весь парсинг
# ниже молча ломается (обнаружено live: после смены языка на русский
# именно это и произошло).
su phablet -c '
export XDG_RUNTIME_DIR=/run/user/1000
export PULSE_RUNTIME_PATH=/run/user/1000/pulse
export LC_ALL=C
_default_sink=$(pactl info 2>/dev/null | sed -n "s/^Default Sink: //p")
pactl list sinks 2>/dev/null | awk -v sink="$_default_sink" "
\$0 ~ \"Name: \" sink \"\$\" {f=1}
f && /Volume:/{match(\$0,/[0-9]+%/); print substr(\$0,RSTART,RLENGTH-1); exit}
"
pactl list sinks 2>/dev/null | awk -v sink="$_default_sink" "
\$0 ~ \"Name: \" sink \"\$\" {f=1}
f && /Mute:/{print \$2; exit}
"
' > /userdata/saved-volume.txt 2>/dev/null
