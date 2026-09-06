#!/bin/sh
# CLAUDE_DEBUG 2026-09-05 (в36): единая точка входа для запуска камеры
# по нажатию иконки -- заменяет отдельный boot-time systemd-сервис
# провайдера на реальный on-demand триггер прямо из droidian-camera.
# desktop's Exec=.
#
# Запускается ОБЫЧНЫМ пользователем (phablet), без root/sudo/polkit --
# на этом rootfs нет пакета sudo вообще (проверено живьём 2026-09-05).
# Поэтому привилегированная часть (kill minimediaservice + restart
# camera-provider, оба требуют root) вынесена в отдельный root-овый
# systemd .path/.service (halium-camera-ondemand-refresh.path/.service),
# а связь между непривилегированным процессом и root-стороной идёт
# через простое touch файла, на который у phablet есть право записи
# (0666, создаётся halium-camera-trigger.conf в tmpfiles.d) -- никакой
# новой privilege-escalation поверхности, кроме самого файла.
#
# GPU-мост (HYBRIS_LD_LIBRARY_PATH -> vendor-lib64/egl + vendor-lib64/hw,
# т.е. libGLESv2_adreno.so/libEGL_adreno.so/eglSubDriverAndroid.so) --
# тот самый фикс из в33 -- применяется ниже через halium-camera-gpu-
# wrapper.sh, который оборачивает сам бинарник droidian-camera. Он НЕ
# чинит саму глубокую EGL-баг-цепочку (see ФИКСЫ_ДЛЯ_В36.txt, "Android
# META-EGL" -- eglGetConfigAttrib возвращает нули для всех 72 конфигов),
# но гарантирует, что нужные .so вообще подключены к процессу камеры.
touch /run/halium-camera-trigger 2>/dev/null || true

# Провайдеру + свежей паре minimediaservice нужно время подняться после
# рестарта, прежде чем сам UI камеры начнёт биндиться к HAL.
sleep 2

exec /usr/libexec/halium-camera-gpu-wrapper.sh /usr/bin/droidian-camera
