# v25: недостающий arm64-пакет pulseaudio-modules-droid-modern (2026-07-27)

## Контекст
Живой флеш v24 (с уже исправленным `--force-overwrite` для
`libasound2-plugins` и рабочим `dpkg-retry`-циклом) всё равно завершил
chroot-установку аудио-пакетов с `WARNING: chroot dpkg install exited 1`.

## Диагностика
`libexpat1`/`libasound2-plugins` в этот раз встали нормально -- retry-
цикл сработал как надо. Единственная persistентная (не устраняемая
повторными `dpkg -i`/`--configure -a`) ошибка в самом конце лога:

```
pulseaudio-modules-droid-hidl depends on pulseaudio-modules-droid-jb2q
| pulseaudio-modules-droid-modern; however:
  Package pulseaudio-modules-droid-jb2q is not installed.
```

`pulseaudio-modules-droid-hidl` -- **arm64**-пакет (без суффикса
`:armhf`). Его OR-зависимость `pulseaudio-modules-droid-modern`
(без architecture qualifier) требует **arm64**-сборку. В логе же
нигде не встречается `pulseaudio-modules-droid-modern:arm64` --
распаковывается и настраивается ТОЛЬКО `:armhf`-вариант. Проверка
`halium-extras/rootfs-files/usr/local/lib/halium-audio-fix-debs/`
подтвердила: там реально лежит только armhf-сборка droid-modern,
arm64-сборки нет вообще, `jb2q` нет ни в каком виде. Это не проблема
порядка установки, а по-настоящему отсутствующий .deb-файл в наборе
из ~290 пакетов (видимо, при первоначальном сборе корпуса пакетов
для halium-audio-fix-debs забыли включить arm64-вариант этого
конкретного пакета).

## Фикс (v25)
Скачан `pulseaudio-modules-droid-modern_14.2.103-1+droidian0+
git20240829212213.935726a.next.production_arm64.deb` напрямую с
`releases.droidian.org` (та же версия, что уже была для armhf --
просто недостающая архитектура той же самой версии), сверен по
SHA256 из индекса Packages, добавлен в
`halium-extras/rootfs-files/usr/local/lib/halium-audio-fix-debs/`
рядом с уже существующим armhf-вариантом.

## Статус
Не протестировано живьём -- нужен новый флеш v25.

## Второй фикс в v25: контейнер больше не стартует сам вообще

Живой тест v24 (ExecStartPre-задержка 50с) снова провалился на первой
загрузке после флеша -- SSH и карта не поднялись. Все три подхода
подряд (v22/v23: ConditionPathExists+таймер, v24: ExecStartPre-задержка)
не дали стабильного результата на первой загрузке после свежего флеша,
хотя каждый по отдельности выглядел логически корректным и был
подтверждён деплоенным на устройство без ошибок. Точная причина сбоя
не установлена (кандидаты: гонка с /userdata, зависание системных
вызовов при первой загрузке -- живьём во время диагностики также
поймали зависший процесс `losetup -a` на устройстве, т.е. подвисания
на этом устройстве бывают и по другим причинам).

По явному запросу вместо очередной автоматики -- контейнер теперь
**никогда** не стартует сам, ни по таймеру, ни по задержке:
- `lxc-android-config.service.d/00-first-boot-gate.conf`:
  `ConditionPathExists=/userdata/CONTAINER_ENABLED` -- маркер, который
  не создаётся никаким автоматическим скриптом.
- `halium-extras/libexec/halium-enable-container.sh` (новый): вручную
  запускается по ssh, когда пользователь сам решил поднять контейнер
  (`touch /userdata/CONTAINER_ENABLED && systemctl start
  lxc-android-config.service`).
- `update-binary`: `rm -f /data/CONTAINER_ENABLED` при каждом флеше --
  та же защита от переживания флага через /userdata, что и для
  PHOSH_FIRST_BOOT_OK выше.
- Удалён `halium-extras/libexec/halium-first-boot-delay.sh` (больше не
  используется).

**НЕ протестировано живьём.**

## Третье дополнение в v25: "как в v21" -- маска в 3 местах прямо в образе

Живой тест v24 (только ExecStartPre-задержка) снова не дал SSH/карту на
первой загрузке. По явному запросу пользователя воспроизведена ТА ЖЕ
самая комбинация, что живьём проверенно сработала ещё на v21 (см.
FIXES-V22-FIRST-BOOT-CONTAINER-GATE.md, "Живой тест: замаскировали
lxc-android-config.service и halium-start-container-delayed.service/
.timer ... Дополнительно нейтрализован halium-restore-state.sh"), но
теперь -- постоянно, прямо в update-binary, а не разовой ручной правкой
через TWRP:

- `ln -sf /dev/null .../halium-start-container-delayed.service` и
  `.timer` -- прямо в update-binary (не в rootfs-files как физический
  симлинк -- `zip -9 -r` без `-y` дереференсит симлинки при сборке
  архива, см. правило проекта). `halium-restore-state.sh` продолжает
  писать в эти пути каждую загрузку, но раз это симлинки на /dev/null,
  запись уходит в никуда, а маска не исчезает -- в отличие от полной
  нейтрализации всего скрипта (как делалось вручную при живой
  диагностике), здесь НЕ теряется остальной функционал
  halium-restore-state.sh (wifi/громкость/локаль).
- `lxc-android-config.service` -- НЕ маскируется тем же способом
  (halium-restore-state.sh делает `rm -f` именно по этому пути каждую
  загрузку, снёс бы симлинк) -- единственная надёжная точка контроля
  для него остаётся `ConditionPathExists=/userdata/CONTAINER_ENABLED`
  в `.service.d/00-first-boot-gate.conf` (см. выше).

**НЕ протестировано живьём.**
