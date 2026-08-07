# v23: полное перекрытие всех путей запуска контейнера + фикс dpkg-порядка (2026-07-27)

## Контекст
Первый живой тест v22 (гейт `PHOSH_FIRST_BOOT_OK` + 55с-таймер + PATH-фикс
для chroot) показал две проблемы.

## Проблема 1: гейт контейнера покрывал не все пути

Диагностика зависшей первой загрузки v22 (TWRP, pstore) показала, что
Android-контейнер всё равно стартовал ДО срабатывания 55с-таймера
(`/userdata/PHOSH_FIRST_BOOT_OK` не существовал, но `init` контейнера уже
работал в pstore-логе).

**Причина**: гейт из v22 был реализован ТОЛЬКО в `halium-restore-
state.sh`, который создаёт (или не создаёт) юниты `halium-start-
container-delayed.service`/`.timer`. Но как минимум **9 других юнитов**
самостоятельно объявляют `Wants=`/`Requires=lxc-android-config.service`
в СВОИХ СОБСТВЕННЫХ `[Unit]`-секциях (все enabled по умолчанию через
`multi-user.target.wants`):
- halium-sensor-bridge-watchdog.service
- halium-fix-backlight-perm.service
- halium-fix-torch-perm.service
- halium-install-sensor-bridge.service
- halium-stop-fps-spam.service
- halium-start-adbd.service
- halium-fix-crashdump-storm.service
- halium-fix-flash-perm.service
- (9-й, найден через `grep -l` count, не выписан поимённо)

Любой из них, стартуя на раннем этапе загрузки, тянул за собой
`lxc-android-config.service` независимо от моего таймера -- гейт
покрывал только ОДИН из множества путей.

**Исправление**: вместо патчинга каждого из 9+ юнитов по отдельности --
единая точка контроля прямо на самом `lxc-android-config.service`:

`halium-extras/rootfs-files/etc/systemd/system/lxc-android-config.
service.d/00-first-boot-gate.conf`:
```ini
[Unit]
ConditionPathExists=/userdata/PHOSH_FIRST_BOOT_OK
```

`ConditionPathExists` проверяется systemd'ом заново при КАЖДОЙ попытке
запуска юнита (не кэшируется) -- независимо от того, кто и как его
запрашивает (явный `systemctl start`, `Wants=`, `Requires=`, таймер).
Пока файла нет, systemd просто пропускает юнит (считает "successfully
skipped", не ошибкой) при любой попытке. Как только 55с-таймер создаёт
маркер -- следующая ЛЮБАЯ попытка запуска (в том же боте или в любом
следующем) проходит нормально. Ограничение действует строго один раз,
на первую загрузку после переflash-а -- ни один из юнитов НЕ требует
отдельного патчинга.

Проверено: конфликтов с уже существующими дроп-инами в этой же папке
(`98-restart-on-failure.conf`, `99-wait-for-ssh.conf`, `override.conf`)
нет -- ни один из них не трогает `Condition*=`.

(По пути также проверены и признаны тупиковыми: `halium-kickstart-
lxc.sh` -- существует, но нигде реально не вызывается ни одним
enabled-юнитом; `halium-lxc-uptime-restart.service` -- его enable-
строка в `update-binary` закомментирована.)

## Проблема 2: dpkg pre-dependency ordering в chroot-установке

См. `FIXES-V22...` -- нет, отдельно задокументировано здесь: PATH-фикс
из v22 сработал (`exit 127` -> `exit 1`), но обнажил вторую проблему --
`libexpat1:armhf` pre-depends на `libc6 (>= 2.38)`, но `libc6:armhf` к
моменту установки `libexpat1` был "unpacked, but has never been
configured" (единственный проход `dpkg -i *.deb` без топологической
сортировки). Каскад: `libexpat1` отклонён целиком -> `fontconfig` ->
`cairo` -> `pango` -> `gdk-pixbuf` -> `pulseaudio-modules-droid` ->
`pulseaudio` -> `gnome-settings-daemon` -> `gnome-control-center` -- все
"leaving unconfigured".

**Исправление** (в `update-binary` и в live-fallback `halium-install-
audio-fix.sh`): повторный проход `dpkg -i --force-confold .../*.deb`
СРАЗУ ПОСЛЕ первого `dpkg --configure -a`, до дополнительных configure:
```sh
dpkg -i --force-confold /usr/local/lib/halium-audio-fix-debs/*.deb 2>&1
dpkg --configure -a 2>&1
dpkg -i --force-confold /usr/local/lib/halium-audio-fix-debs/*.deb 2>&1
dpkg --configure -a 2>&1
dpkg --configure -a 2>&1
```
На втором проходе `libc6:armhf` уже сконфигурирован (первым
`--configure -a`), ранее отклонённые пакеты встают нормально.

## Статус (обновлено в v24)
Оба фикса синтаксически проверены (`sh -n` для shell-скриптов; .conf --
проверен визуально + отсутствие конфликтов с соседними дроп-инами).
Живой тест v23 подтвердил: container-gate и dpkg-retry фиксы сработали
(флаш прошёл, RC=0), НО обнажили третью, отдельную проблему -- см. ниже.

## Проблема 3 (найдена в живом тесте v23): libasound2-plugins shared-file конфликт

chroot dpkg install всё ещё падал (exit 1) после dpkg-retry фикса.
Разбор `/data/halium-audio-fix-chroot-install.log` с v23 показал НОВУЮ
причину (не связана с libc6/libexpat1 ordering): `libasound2-
plugins:arm64` отказывается ставиться --
"trying to overwrite shared '/etc/alsa/conf.d/99-pulseaudio-default.
conf.example', which is different from other instances of package
libasound2-plugins:arm64" -- arm64 и armhf сборки этого пакета (из
droidian-rolling снапшота) содержат РАЗНОЕ содержимое номинально общего
файла. Единственный такой случай в логе.

**Исправление (v24)**: добавлен `--force-overwrite` к обоим `dpkg -i
--force-confold` вызовам в `update-binary` (строки ~929, ~931) и в
live-fallback `halium-install-audio-fix.sh`. Безопасно -- затронутый
файл это `.example`-образец, не активный конфиг.

**НЕ протестировано живьём** -- следующий flash v24 будет первым тестом
всех четырёх фиксов разом (chroot-PATH из v22 + dpkg-retry + полный
container-gate + force-overwrite для libasound2-plugins).

## Проблема 4 (найдена в живом тесте v23): флаг PHOSH_FIRST_BOOT_OK пережил флеш

Живой флеш v23 показал: контейнер всё равно стартовал сразу на первой
загрузке, гейт (55с-таймер) не сработал вообще, хотя в v23 он покрывал
уже ВСЕ пути (Проблема 1) через `ConditionPathExists` на самом
`lxc-android-config.service`.

Причина оказалась не в покрытии путей, а в самом флаге: `/userdata`
(физический раздел `/dev/block/sda34`) флеш архива НЕ стирает. Файл
`/data/PHOSH_FIRST_BOOT_OK` (= `/userdata/PHOSH_FIRST_BOOT_OK` в
рантайме) остался от УСПЕШНОЙ загрузки v22 (когда 55с-таймер сработал
и выставил флаг в true) -- подтверждено датой файла и содержимым
`/data/halium-first-boot-gate.log`:
```
19:48:55: PHOSH_FIRST_BOOT_OK not set yet -- ... arming 55s first-boot timer
19:49:45: 55s first-boot timer fired -- setting PHOSH_FIRST_BOOT_OK ...
```
При флеше v23 этот старый флаг остался на диске, `halium-restore-
state.sh` увидел его сразу и пошёл по обычной ветке (контейнер стартует
немедленно) -- гейт для НОВОЙ прошивки просто не включился.

Смысл флага -- "первый запуск ПОСЛЕ ЭТОЙ прошивки", а не "первый
запуск с рождения этого userdata", поэтому каждый новый флеш обязан
сбрасывать его сам.

**Исправление (v24)**: в `update-binary`, сразу после копирования
`halium-restore-state.sh` на `/data`, добавлено:
```sh
rm -f /data/PHOSH_FIRST_BOOT_OK
rm -f /data/halium-first-boot-gate.log
```
Теперь каждый флеш безусловно сбрасывает флаг, гейт срабатывает заново
на первой загрузке новой версии независимо от истории предыдущих
версий на этом же userdata.

**НЕ протестировано живьём** -- нужен новый флеш v24 (архив уже собран
без этого фикса, требует пересборки).

## Проблема 5 (v24): весь механизм гейта первой загрузки переделан на ExecStartPre

Живой тест v23 (с фиксом Проблемы 4 внутри update-binary, но ещё СО
СТАРЫМ ConditionPathExists+отдельный-таймер гейтом) показал: контейнер
всё равно стартует на первой загрузке и ломает SSH -- пользователь
подтвердил (после переформатирования /data дважды, так что залипания
флага НЕ было): "та логика что сейчас есть в архивах работает со 2 го
запуска, а первый никак не проходит по той логике".

**Решение**: полностью убран механизм ConditionPathExists +
отдельный `halium-first-boot-container-start.service/.timer`.
Причина ненадёжности до конца не установлена (вероятно конфликт с
прямым симлинком `multi-user.target.wants/lxc-android-config.service`
из ЗАДАЧИ 14 DROIDIAN-V23-PLAN.md, либо гонка с /userdata, либо общая
подтверждённая ненадёжность независимых юнитов-таймеров на первой
загрузке -- см. также похожий живой баг с
halium-pulseaudio-sink-watchdog.timer, задокументированный в самом
halium-restore-state.sh). Вместо гадать дальше -- взят ЖИВЬЁМ
ПРОВЕРЕННЫЙ приём из ЗАДАЧИ 11 DROIDIAN-V23-PLAN.md (2026-07-24,
`halium-wait-network-before-lxc.sh`, `sleep 80` -- "первая полная
загрузка до рабочего стола Phosh на чистом /data за всю историю
сессии"), но никогда не закоммиченный в репозиторий (жил только
живьём на устройстве, потерян при следующем флеше).

Новая схема (v24):
- `halium-extras/libexec/halium-first-boot-delay.sh`: проверяет
  `/userdata/PHOSH_FIRST_BOOT_OK`; если нет -- `sleep 50`, `touch`
  маркер, exit 0; если есть -- exit 0 сразу.
- `lxc-android-config.service.d/00-first-boot-gate.conf`:
  `ExecStartPre=/usr/libexec/halium-first-boot-delay.sh` (вместо
  `ConditionPathExists=`).
- `halium-restore-state.sh`: убран весь if/else -- теперь БЕЗУСЛОВНО
  (каждую загрузку одинаково) создаёт обычный
  `halium-start-container-delayed.service/.timer`, как было до всей
  этой истории с гейтом. `halium-first-boot-container-start.service/
  .timer` и `halium-extras/libexec/halium-first-boot-container-start.sh`
  полностью удалены.

**Почему это надёжнее**: `ExecStartPre=` -- часть исполнения САМОГО
юнита `lxc-android-config.service`. systemd гарантированно не запустит
`ExecStart` (реальный подъём контейнера), пока `ExecStartPre` не
завершится -- это работает независимо от того, КТО и КАК запросил
старт юнита (явный симлинк, чужой Wants=/Requires=, ручной `systemctl
start`), в отличие от `ConditionPathExists` (просто помечает job
"skipped", но не защищает от гонок с /userdata на разных путях запуска)
и от отдельного timer-юнита (лишняя независимая сущность, у которой
была своя, до конца не понятая причина ненадёжности именно на первой
загрузке -- confirmed дважды подряд, v22 и v23).

`rm -f /data/PHOSH_FIRST_BOOT_OK` при каждом флеше (Проблема 4 выше)
остаётся в силе -- гейт по-прежнему обязан сбрасываться на каждой новой
прошивке.

**НЕ протестировано живьём** -- следующий флеш v24 будет первым тестом
новой схемы.
