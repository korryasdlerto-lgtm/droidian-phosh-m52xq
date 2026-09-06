#!/bin/sh
# CLAUDE_DEBUG 2026-09-05 (в36): триггерится halium-camera-ondemand-
# refresh.path при каждом касании /run/halium-camera-trigger (см.
# halium-camera-launch.sh, вызывается из Exec= droidian-camera.desktop).
#
# CLAUDE_DEBUG 2026-09-05, ТРЕТЬЕ исправление -- откат отдельного
# постоянно-живого halium-minimediaservice-direct-start.service (см.
# ФИКСЫ_ДЛЯ_В36.txt п.14.9 и halium-camera-provider-direct-start.sh):
# по прямому указанию пользователя, воспроизводящему живьём проверенную
# в34-схему -- kill minimediaservice теперь ТОЛЬКО внутри самого
# провайдерского скрипта, никакой отдельной supervision. Здесь остаётся
# только `start` (идемпотентно) самого провайдера.
systemctl start halium-camera-provider-direct-start.service

# CLAUDE_DEBUG 2026-09-06 (в38): halium-fix-libcameraservice.service
# перенесён с boot-time (After=phosh.service) на on-demand -- по той же
# логике, что и весь остальной этот файл: незачем трогать камеру-
# специфичные файлы при КАЖДОЙ загрузке, если камеру ни разу не открыли.
# Юнит Type=oneshot + RemainAfterExit=yes, так что `systemctl start`
# идемпотентен -- реальная запись в контейнер происходит только при
# первом открытии камеры за загрузку, повторные открытия -- no-op.
systemctl start halium-fix-libcameraservice.service
