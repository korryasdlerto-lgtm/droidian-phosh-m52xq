#!/bin/sh
# CLAUDE_DEBUG 2026-09-05 (в36, переход на on-demand): вопреки тому, что
# делают halium-kick-camera-service.sh/halium-*-watchdog.sh (используют
# `stop vendor.camera-provider-2-6; start vendor.camera-provider-2-6`),
# ЭТО НЕ РАБОТАЕТ на этом устройстве -- запатченный init не распознаёт
# это имя сервиса вообще, ctl.start/ctl.stop для него -- no-op (проверено
# живьём: PID-список провайдера не менялся до/после stop+start).
# Единственный подтверждённый рабочий способ -- прямой exec бинарника в
# обход init полностью. Этот скрипт делает именно это.
#
# CLAUDE_DEBUG 2026-09-05: раньше (в33/в34) этот скрипт стартовал сам по
# себе при каждой загрузке (WantedBy=multi-user.target) и содержал
# ожидание "phosh.service стал active" + sleep 10 -- это было нужно
# ТОЛЬКО чтобы не добавлять binder/HAL-трафик в то же окно, где phoc
# боролся с surfaceflinger за composer (см. историю в ФИКСЫ_ДЛЯ_В34.txt
# п.12.2/12.3). Теперь провайдер стартует ИСКЛЮЧИТЕЛЬНО по требованию --
# systemctl restart дергается из halium-camera-ondemand-refresh.service,
# который сам триггерится .path-юнитом при нажатии иконки Камеры (см.
# halium-camera-launch.sh). К этому моменту рабочий стол уже точно
# поднят пользователем -- ждать больше нечего, весь wait-loop убран.
#
# CLAUDE_DEBUG 2026-09-05, ИСПРАВЛЕНО после первого живого теста: `exit
# status=134` (SIGABRT) в цикле на каждом `systemctl restart` -- живьём
# найдено, что `systemctl restart` убивает только ВНЕШНИЙ lxc-attach-
# обёртку, а сам бинарник провайдера ВНУТРИ пространства имён контейнера
# остаётся жить (виден живьём как отдельный PID через `lxc-attach --
# pgrep`) и продолжает держать регистрацию HAL-сервиса в binder'е --
# новый экземпляр падает по SIGABRT, пытаясь зарегистрироваться повторно
# под тем же именем. Тот же класс проблемы, что и с minimediaservice
# (см. halium-minimediaservice-direct-start.service) -- lxc-attach
# ломает обычное systemd-supervision, потому что реальный процесс живёт
# в ДРУГОМ пространстве имён. Убиваем старый экземпляр явно перед
# каждым exec, тем же паттерном, что раньше уже применялся для
# minimediaservice.
lxc-attach -n android -- pkill -9 -f "camera.provider@2.6-service" 2>/dev/null || true
#
# CLAUDE_DEBUG 2026-09-05, ВОЗВРАТ к оригинальной в34-схеме: отдельный
# постоянно-живой halium-minimediaservice-direct-start.service (введён
# сегодня раньше) убран из цепочки -- живьём подтверждено пользователем
# ("ты делал это вручную, без перезагрузки, повтори") и по документации
# в34 (ФИКСЫ_ДЛЯ_В34.txt п.12.6-12.7): рабочая схема -- ПРОСТОЙ pkill
# minimediaservice прямо здесь, без постоянной systemd-supervision,
# БЕЗ попытки гарантировать его "живость" наперёд. Мой более ранний
# вывод "Android init не поднимает minimediaservice сам" был построен на
# сломанном тесте (`pgrep -x minimediaservice` жертва бага усечения
# имени в ядре, TASK_COMM_LEN=16 -- см. ФИКСЫ_ДЛЯ_В36.txt п.14.9) --
# постоянная supervision, возможно, как раз МЕШАЕТ провайдеру создать
# корректную пару при реальной съёмке. Оставляем ТОЛЬКО этот kill.
lxc-attach -n android -- pkill -9 -x minimediaservice 2>/dev/null || true
sleep 1
exec lxc-attach -n android -- /vendor/bin/hw/android.hardware.camera.provider@2.6-service_64.samsung-sm7325
