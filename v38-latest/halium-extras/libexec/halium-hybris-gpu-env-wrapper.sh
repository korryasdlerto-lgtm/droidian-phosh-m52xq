#!/bin/sh
# Универсальная обёртка (замена halium-firefox-gpu-wrapper.sh) --
# та же причина: любое приложение, запускаемое через app-grid Phosh
# (транзитный .scope-юнит "Application launched by phosh" -- Phosh
# форкает процесс НАПРЯМУЮ, systemd просто оборачивает уже запущенный
# PID, Environment= юнита тут в принципе ни на что повлиять не может)
# теряет HYBRIS_LD_LIBRARY_PATH/ANDROID_ROOT, даже когда они есть в
# environment.d и в /proc/self/environ самого Phosh -- Phosh явно не
# пробрасывает их дальше при запуске приложений. Единственный рабочий
# фикс -- обёрточный бинарник в самом Exec= .desktop-файла, для
# КАЖДОГО приложения отдельно (подтверждено живьём -- firefox-esr
# заработал именно так; epiphany имел ТУ ЖЕ ошибку "libEGL.so"/
# "libGLESv2.so not found" со своим отдельным .desktop-файлом).
#
# Использование: halium-hybris-gpu-env-wrapper.sh /путь/к/реальному/бинарнику арг1 арг2 ...
export ANDROID_ROOT=/android
export HYBRIS_LD_LIBRARY_PATH=/opt/halium-lxc-bridge/system-lib64:/opt/halium-lxc-bridge/vendor-lib64:/android/system/system/apex/com.android.i18n/lib64:/android/system/system/apex/com.android.i18n/lib
exec "$@"
