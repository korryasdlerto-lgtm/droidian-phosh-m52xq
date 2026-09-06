#!/bin/sh
# НАЙДЕНО 2026-09-02 живьём: Firefox (как и любое приложение,
# запускаемое через app-grid Phosh) получает своё окружение НЕ через
# systemd (транзитный юнит "app-gnome-*.scope" -- это просто ОБЁРТКА
# systemd вокруг УЖЕ ЗАПУЩЕННОГО процесса, RegisterMachine-подобная
# семантика, cgroup/lifecycle-only, Environment= у .scope-юнита в
# принципе не может повлиять на процесс, который сам себя уже
# отфоркал) -- окружение целиком определяется тем, что сам Phosh
# (точнее GLib/GAppLaunchContext внутри него) передаёт при fork+exec,
# и он ЯВНО не пробрасывает HYBRIS_LD_LIBRARY_PATH дальше, даже когда
# он есть в /proc/self/environ самого Phosh. Ни systemd environment.d,
# ни .scope-дропин это не лечат в принципе -- нужен именно обёрточный
# бинарник в самом Exec= .desktop-файла.
export ANDROID_ROOT=/android
export HYBRIS_LD_LIBRARY_PATH=/opt/halium-lxc-bridge/system-lib64:/opt/halium-lxc-bridge/vendor-lib64:/android/system/system/apex/com.android.i18n/lib64:/android/system/system/apex/com.android.i18n/lib
exec /usr/lib/firefox-esr/firefox-esr "$@"
