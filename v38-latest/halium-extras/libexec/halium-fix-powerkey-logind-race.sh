#!/bin/sh
# НАЙДЕНО 2026-09-01/02 живьём: logind не видел power-key устройство
# (event0, driver qpnp_pon) вообще -- `loginctl seat-status seat0` не
# перечислял его среди устройств seat0, несмотря на корректный udev-
# тег (`TAGS=:power-switch:`, подтверждено `udevadm info`). Raw evdev-
# событие (KEY_POWER press/release) доходило до ядра идеально чисто
# (подтверждено прямым чтением /dev/input/event0) -- обрыв был именно
# между udev-тегом и тем, что logind реально СЧИТАЛ устройство своим
# при перечислении seat0 на старте. Похоже на гонку: logind
# перечисляет seat0-устройства при своём собственном старте, а тег
# power-switch на этом конкретном узле мог примениться чуть позже.
#
# ФИКС: `udevadm trigger --action=add` для всего input-подсистема --
# безопасно, идемпотентно (просто повторно рассылает uevent "add" для
# уже существующих устройств), logind слушает udev monitor в реальном
# времени и подхватывает устройство без необходимости своего рестарта
# (рестарт logind живьём с активной сессией phoc -- ЗАПРЕЩЁН отдельным
# правилом проекта, ломает текущую seat-сессию).
#
# ПОДТВЕРЖДЕНО ЖИВЬЁМ: после этого `loginctl seat-status seat0`
# начинает показывать qpnp_pon/input0, и `journalctl` показывает
# "Power key pressed short." при каждом нажатии. Полная цепочка
# (блокировка/разблокировка экрана самим Phosh) при этом ВСЁ РАВНО не
# заработала до конца -- отдельный, более глубокий баг в реакции
# самого Phosh на инхибированное событие от logind, НЕ пофикшен этим
# скриптом. Оставлено как частичный, но реальный и подтверждённый шаг.
LOG_TAG="halium-fix-powerkey-logind-race"
echo "$LOG_TAG: triggering udev re-add for input subsystem" | systemd-cat -t "$LOG_TAG" -p info
udevadm trigger --action=add --subsystem-match=input
