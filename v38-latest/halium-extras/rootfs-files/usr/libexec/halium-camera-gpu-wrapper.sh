#!/bin/sh
export ANDROID_ROOT=/android
export LD_LIBRARY_PATH=/android/system/lib64:/android/system/system/lib64
# FIXES-V32 2026-09-04: подпапки egl/ и hw/ ОБЯЗАТЕЛЬНЫ -- настоящие
# GPU-драйверы (libGLESv2_adreno.so, libEGL_adreno.so,
# eglSubDriverAndroid.so) лежат именно там, а HYBRIS_LD_LIBRARY_PATH
# НЕ рекурсирует в подпапки. Найдено в параллельном UT-проекте
# (ФИКСЫ_ДЛЯ_В50.txt) -- там это была причина того же чёрного
# видоискателя и тех же "library ... not found" в логе.
export HYBRIS_LD_LIBRARY_PATH=/opt/halium-lxc-bridge/system-lib64:/opt/halium-lxc-bridge/vendor-lib64:/opt/halium-lxc-bridge/vendor-lib64/egl:/opt/halium-lxc-bridge/vendor-lib64/hw:/android/system/system/apex/com.android.i18n/lib64:/android/system/system/apex/com.android.i18n/lib
export QT_QUICK_BACKEND=software
export QT_QPA_PLATFORM=wayland
export QT_MULTIMEDIA_PREFERRED_PLUGINS=AalServicePlugin
exec "$@"
