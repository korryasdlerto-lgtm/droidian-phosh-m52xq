# v27: недостающие arm64-пакеты аудио-стека + весь путь диагностики "нет SSH/рабочего стола" (2026-09-01)

## Контекст

Живая прошивка v25, затем v26 (полный вайп Data, чистая установка с нуля).
Цель сессии: довести Droidian/Phosh на m52xq до стабильно загружающегося
состояния (SSH + рабочий стол) без ручных live-патчей после каждого флеша.

Все ниже перечисленные баги были сначала найдены и пофикшены ЖИВЬЁМ на
уже прошитом v25/v26 (через overlay `/userdata/rootfs-overlay/...`,
без переустановки), затем перенесены в исходники — сначала по ошибке
частично в v26 (v26 УЖЕ был собран и прошит на момент находки бага 4,
поэтому v27 создан по обычному принципу проекта "не трогать
уже прошитый архив").

## Баг 1 (уже был известен, довнесён в v26 живьём) — `usb-moded.service`
StartLimitBurst по умолчанию (5 попыток/10 сек) исчерпывался почти
мгновенно из-за что-то ещё падающего в цепочке (см. баг 2) —
usb-moded переставал стартовать вообще на всю оставшуюся загрузку,
gadget (`g1`) никогда не создавался, SSH/карта не поднимались.
Подтверждено live через pstore: 5 попыток за ~5.5 сек, затем полная
тишина до самого ребута.

**Фикс**: `halium-extras/rootfs-files/etc/systemd/system/usb-moded.service.d/
zz-startlimit-retry.conf` — `StartLimitIntervalSec=120`, `StartLimitBurst=30`,
`RestartSec=2`. Тот же самый фикс, что уже был найден и подтверждён
на ДРУГОМ (Ubuntu Touch) проекте того же устройства
(см. память `project_halium_ssh_boot_chain_2026-07-29`).

## Баг 2 (уже был известен, довнесён в v26 живьём) — `usb-moded-ssh.service`
`RestartPreventExitStatus=255` в базовом юните блокирует ЛЮБОЙ retry
именно на exit-код 255 — а rescue sshd на 8022 порту иногда падает с
exit 255 на самой первой попытке (пытается забиндиться на
`10.15.19.82:8022` до того как usb0-интерфейс реально сконфигурирован).

**Фикс**: `halium-extras/rootfs-files/usr/lib/systemd/system/
usb-moded-ssh.service.d/zz-allow-retry-on-255.conf` — очищает
`RestartPreventExitStatus=`, `Restart=on-failure`, `RestartSec=2`.

## Баг 3 (НОВЫЙ, найден в этой сессии) — `getprop` не мог загрузить `libhybris-common.so.1` (arm64)

### Диагностика
С фиксами 1-2 usb-moded стал ретраиться бесконечно (не сдавался после
5 попыток), но `g1`-гейджет всё равно НИКОГДА не создавался -- "g1 not
found - fresh start" повторялось на каждой из 30+ живых попыток за один
boot. Добавлен временный live-патч в `usr/libexec/usb-udc-cleanup.sh`
(перехват stdout/stderr настоящего `ubports-usb-moded-configurator` в
`/dev/kmsg`, т.к. persistent journal на этом устройстве ненадёжен --
"Device or resource busy" при ротации, не переживает reboot). Это сразу
показало точную ошибку:

```
getprop: error while loading shared libraries: libhybris-common.so.1:
cannot open shared object file: No such file or directory
```

Корень: существующий drop-in `zz-hybris-bridge-libs.conf` задавал ТОЛЬКО
`HYBRIS_LD_LIBRARY_PATH` (фидит внутренний dlopen() самого hybris), но
НЕ обычный `LD_LIBRARY_PATH` (нужен для резолва DT_NEEDED плейн-ELF
бинарника `getprop` через настоящий ld.so) -- тот же самый баг и тот же
фикс, что уже когда-то нашли и закрыли на другом (Ubuntu Touch) проекте
этого устройства (см. память `project_halium_ssh_boot_chain_2026-07-29`,
"Bug 2 correction"), просто забыли перенести сюда.

Но даже с `LD_LIBRARY_PATH` добавленным, живой TWRP loop-mount `rootfs.img`
(`-o ro,noload`, безопасно, без journal replay) показал: файла
`/usr/lib/aarch64-linux-gnu/libhybris-common.so.1` физически НЕТ на
диске. При этом `dpkg -l libhybris-common1` продолжает считать пакет
`ii` (installed) -- несовпадение между dpkg-базой и реальным
содержимым файловой системы.

### Настоящая причина
`halium-audio-fix-debs` (набор ~290 .deb для аудио-стека,
устанавливается через `chroot`+`dpkg -i` ПРЯМО В rootfs.img на этапе
флеша, до включения overlay -- чтобы избежать отдельного, уже
задокументированного overlayfs-whiteout бага) содержит
`libhybris-common1` **только для armhf**. Живой лог
`/data/halium-audio-fix-chroot-install.log` показал:
```
dpkg: warning: downgrading libhybris-common1 (...git20260715172057...)
to (...git20260707121655...)
```
-- т.е. пакет реально downgrade'ится/переустанавливается ходом
транзакции (конфликт версий между уже стоящей в базовом
Droidian-rolling образе версией и версией из нашего бандла), и по ходу
этого процесса arm64-вариант файла теряется целиком, хотя dpkg
почему-то продолжает верить, что он "ii" (возможно, из-за
multiarch-специфики транзакции: армхф-переустановка задевает общий
пакет-неймспейс).

### Временный live-фикс (уже НЕ актуален для этого дока, только для истории)
Извлечён `.deb` из `/var/cache/apt/archives/libhybris-common1_...
git20260715172057...arm64.deb` (apt хранит кэш уже установленных
пакетов) -- это ОРИГИНАЛЬНАЯ, дефлешевая версия, что стояла в
базовом образе ДО нашего audio-fix-debs бандла. Файлы
`libhybris-common.so.1.0.0`+`.so.1`-симлинк и
`libhybris/linker/{n,q,o,mm}.so` (тот же пакет, дополнительные
"hybris linker"-плагины -- без НИХ `ubports-usb-moded-configurator`
идёт дальше, но падает с ДРУГОЙ похожей ошибкой:
`Failed to load hybris linker for Android SDK version 29: .../q.so:
No such file`) были восстановлены живьём через overlay upper
(`/userdata/rootfs-overlay/usr/lib/aarch64-linux-gnu/...`).

## Баг 4 (НОВЫЙ, тот же класс) — `phoc` не мог загрузить `libhardware.so.2` (arm64)

После бага 3 контейнер/getprop заработали, но `phosh.service` крутился
в бесконечном bounce-composer restart-цикле (`systemctl` restart
counter дошёл до 28+ живьём). `journalctl -u phosh.service` показал:
```
/usr/bin/phoc: error while loading shared libraries: libhardware.so.2:
cannot open shared object file: No such file or directory
```
Тот же самый класс бага: `libhardware2` (Debian-пакет, ставит
`libhardware.so.2`) есть в `halium-audio-fix-debs` ТОЛЬКО для armhf.
Arm64-сборка вообще отсутствовала в бандле с самого начала (это
подтверждено ещё в v25: см. `FIXES-V25-MISSING-DROID-MODERN-ARM64.md`
-- та находка была про `pulseaudio-modules-droid-modern`, у которого
ЭТА же самая `libhardware2` -- прямая зависимость; починка тогда
осталась неполной, саму `libhardware2:arm64` не добавили).

Скачан `libhardware2_..._arm64.deb` напрямую с индекса пакетов
`releases.droidian.org/snapshots/next/dists/rolling/main/binary-arm64/
Packages.gz`, версия НОВЕЕ чем в bundle
(`git20260812235837.91f805b` vs `git20260707121655.0fa4b70` у armhf) --
взята именно эта, единственная доступная в текущем снапшоте репозитория,
SHA256 сверен по индексу. Восстановлен живьём той же схемой
(`/userdata/rootfs-overlay/usr/lib/aarch64-linux-gnu/libhardware.so.2(.0.0)`).

**После бага 4 -- сброс dentry-кэша ядра потребовался отдельно**
(`echo 2 > /proc/sys/vm/drop_caches`) -- добавление файла напрямую в
upperdir overlayfs, пока overlay уже смонтирован и активен, не всегда
подхватывается на лету (ядро могло закэшировать негативный dentry с
предыдущей неудачной попытки открыть файл). После drop_caches --
`phoc` сразу поднялся, реальный GPU-рендеринг
(`GL renderer: Adreno (TM) 642L`, hwcomposer backend), `gnome-session-
wayland@phosh.target` стартанул, рабочий стол появился на экране
(живой тест подтверждён пользователем).

**Отдельная деталь**: overlay upperdir path МЕНЯЕТСЯ в зависимости от
того, откуда смотреть:
- живьём (SSH в загруженной ОС): `/userdata/rootfs-overlay/...`
- через TWRP (`mount -t ext4 rootfs.img`): `/data/rootfs-overlay/...`
- сам overlayfs-mount изнутри показывает `upperdir=/tmpmnt/rootfs-overlay`
  -- путь из initramfs-стадии, НЕ существующий/недоступный из уже
  загруженной ОС напрямую (initramfs `/tmpmnt` был временной точкой
  монтирования до pivot_root, оригинальный mountpoint не сохраняется
  в новом namespace). Писать файлы напрямую в overlay нужно ТОЛЬКО
  через `/userdata/rootfs-overlay/...` (снаружи, из загруженной ОС) или
  `/data/rootfs-overlay/...` (снаружи, из TWRP) -- никогда не пытаться
  резолвить `/tmpmnt/...` напрямую, такого пути просто нет.

## Фикс в v27 (постоянный, вместо raw-.so safety-net из v26)

v26 (уже собран и прошит на момент находки бага 4 -- по правилам
проекта не трогается задним числом) получил ТОЛЬКО `cp -f` safety-net
в `update-binary` (копирует голые `.so`-файлы ПОСЛЕ chroot dpkg-инсталла,
в обход dpkg вообще). Это работает для загрузки/десктопа, но dpkg
продолжает считать `libhardware2:arm64`/`libhybris-common1:arm64`/
`pulseaudio-modules-droid-modern:arm64` НЕ установленными в своей базе
(`iU`/отсутствуют в `dpkg -l`) -- при живой проверке звук всё-таки
заработал (видимо, армхф-вариант `pulseaudio-modules-droid-modern`
фактически используется рантаймом), но это ХРУПКОЕ, необъяснённое
состояние, которое может сломаться на будущем `apt upgrade`/
`dpkg --configure -a`.

**В v27 сделано правильно**: все 3 недостающих `.deb` (`libhardware2`,
`libhybris-common1`, `pulseaudio-modules-droid-modern`, все arm64,
скачаны с `releases.droidian.org`/извлечены из живого apt-кэша,
SHA256 сверены) добавлены ПРЯМО в
`halium-extras/rootfs-files/usr/local/lib/halium-audio-fix-debs/`
рядом с уже существующими armhf-версиями -- теперь они ставятся
ШТАТНЫМ `dpkg -i`-проходом chroot-установки вместе со всем остальным
аудио-стеком, dpkg-база остаётся консистентной с реальным диском.
Raw-.so safety-net (`cp -f` в `update-binary`) ОСТАВЛЕН как есть,
как дополнительная страховка -- не мешает, срабатывает только если
файла ещё нет.

## Статус
- Баги 1-2: подтверждены живьём в v25/v26 (усб-модед стабильно
  стартует, ретраится долго вместо мгновенной сдачи).
- Баг 3: подтверждён живьём в v25/v26 (getprop работает, контейнер
  поднимается: zygote64 + system_server + servicemanager/
  hwservicemanager/vndservicemanager все живы).
- Баг 4: подтверждён живьём в v25/v26 -- **рабочий стол реально
  показан на экране устройства**, GPU-рендеринг через Adreno 642L,
  подтверждено пользователем визуально.
- v27 (3 arm64 .deb в халиум-audio-fix-debs вместо raw-.so):
  **НЕ протестировано живьём** -- собрать zip и прошить с чистого
  вайпа Data для полной проверки.

## Баг 5 (НОВЫЙ, найден на живом v26 после полного вайпа Data) — `pulseaudio.service` SIGSEGV race с контейнером

После полного вайпа Data + чистой прошивки v26 (гейт `lxc-android-
config.service` по дизайну не стартует сам, только по маркеру
`/userdata/CONTAINER_ENABLED`) звук не работал ("Соединение отвергнуто"
на `pactl info`). `systemctl --user status pulseaudio.service` показал:
```
Process: ... ExecStart=/usr/bin/pulseaudio ... (code=killed, signal=SEGV)
pulseaudio.service: Start request repeated too quickly.
pulseaudio.service: Failed with result 'signal'.
```
`pulseaudio.service` -- пользовательский юнит, стартует независимо от
`lxc-android-config.service` (никакого `After=`/ожидания нет вообще).
На самой первой попытке (сразу после логина phablet, контейнер ещё не
запущен вручную) реальный Android/vendor-мост недоступен -- pulseaudio
падает по SIGSEGV, systemd's ДЕФОЛТНЫЙ (не переопределённый нигде)
`StartLimitBurst=5` исчерпывается почти мгновенно, юнит остаётся
мёртвым НАВСЕГДА до ручного вмешательства (`systemctl --user restart`).
`halium-pulseaudio-sink-watchdog.sh` (уже существующий, на таймере) тут
не спасает -- он проверяет "default sink = auto_null" (мягкий сбой), а
не "юнит вообще не запущен" (жёсткий сбой из-за исчерпанного лимита).

Живой фикс: `systemctl --user restart pulseaudio.service` вручную ПОСЛЕ
того как контейнер уже поднят -- сразу заработало,
`pactl list short sinks` показал реальные `module-droid-card.c` синки
(`sink.primary_output`, `sink.fast`), не `auto_null`.

**Постоянный фикс (в v27)**: `halium-extras/rootfs-files/etc/systemd/
user/pulseaudio.service.d/zz-startlimit-retry.conf` --
`StartLimitIntervalSec=180`/`StartLimitBurst=20`/`RestartSec=3`, тот же
принцип что и `usb-moded.service`'s `zz-startlimit-retry.conf` (баг 1
выше). Задеплоен и live (overlay), и в исходники v27.

**Важно для следующего теста**: этот баг проявляется ТОЛЬКО на первой
загрузке ПОСЛЕ полного вайпа Data (когда `CONTAINER_ENABLED`-маркер
ещё не создан, контейнер не автостартует). На всех последующих
загрузках (маркер уже стоит, контейнер стартует сам) pulseaudio,
скорее всего, тоже стартует раньше готовности моста -- нужно
подтвердить живьём, спасает ли увеличенный `StartLimitBurst` сам по
себе, или всё ещё нужен `halium-pulseaudio-sink-watchdog.sh` вдобавок.

## Дополнение: 4-я попытка автоматизировать первый запуск контейнера

По явному запросу пользователя ("это уже 3 раза не получалось, но,
может, дело было в usb-moded, а не в самом таймере -- попробуем ещё
раз") добавлена автоматизация ручного шага "подожди пока SSH/раб.стол
точно живы, потом touch CONTAINER_ENABLED + systemctl start" -- но
СТРОГО только для первой загрузки после флеша, с защитой от повторного
срабатывания:

- `halium-extras/libexec/halium-first-boot-container-autostart.sh` --
  1-в-1 та же логика, что уже делали руками весь сеанс.
- `halium-extras/units/halium-first-boot-container-autostart.timer` --
  `OnBootSec=45s`, без `OnUnitActiveSec` (не повторяется в рамках одной
  загрузки).
- `halium-extras/units/halium-first-boot-container-autostart.service` --
  `ConditionPathExists=!/userdata/CONTAINER_ENABLED` -- ЕДИНСТВЕННАЯ
  точка защиты от повторного срабатывания на будущих загрузках (маркер
  персистентный, `/userdata`).
- Включено в `update-binary` (`timers.target.wants`), рядом с
  остальными периодическими вотчдогами.

**Важно**: v22/v23 (`ConditionPathExists`+свой таймер) и v24
(`ExecStartPre`-задержка) -- это была ТА ЖЕ САМАЯ идея, обе попытки
откатили после живых сбоев (SSH/карта не поднимались на первой
загрузке). В тот момент баг 1 этого дока (usb-moded StartLimitBurst)
ещё не был известен -- вполне возможно, что настоящая причина тех
сбоев была именно в нём (usb-moded молча умирал на всю загрузку из-за
чего-то ещё падающего в цепочке рано, что выглядело неотличимо от "не
поднялся SSH из-за таймера контейнера"). Теперь, когда баг 1
пофикшен, есть реальный шанс, что автоматизация наконец сработает --
но это ПОКА НЕ ПРОВЕРЕНО ЖИВЬЁМ (нужен полный вайп Data + чистая
прошивка v27 для честного теста, у уже прошитого v26 маркер
`CONTAINER_ENABLED` уже стоит, тест на нём ничего не докажет).

## Дополнение 2: автостарт контейнера НА ВСЕХ последующих загрузках тоже был чисто ручным

Живой тест на v26 после реального ребута (не первого после вайпа --
маркер `CONTAINER_ENABLED` уже стоял с предыдущего раза) показал:
контейнер НЕ поднялся сам, `systemctl is-active lxc-android-config.
service` -- `inactive (dead)`. Причина: `ConditionPathExists` в
`00-first-boot-gate.conf` -- это только РАЗРЕШЕНИЕ на старт по запросу,
сама по себе она НИЧЕГО не запускает. Ничто не подавало сам запрос
`systemctl start` -- значит ручной запуск требовался на КАЖДОЙ
загрузке, а не только на первой, что не было очевидно до этого
живого теста.

Оказалось, что штатный механизм для "запросить старт после каждой
загрузки" уже существовал в проекте --
`halium-start-container-delayed.service`/`.timer` (генерируется
`halium-restore-state.sh` на `/userdata` каждую загрузку) -- но был
ЗАМАСКИРОВАН на `/dev/null` ещё в v25 (см. "v25: та же самая '3 места'
маскировка" выше), потому что на момент той маскировки контейнер вообще
не должен был стартовать сам НИКОГДА, ни на первой загрузке, ни на
последующих.

**Фикс в v27** (по явному запросу пользователя, финальная схема --
"при первом старте только наш автостарт на 45с, до этого
halium-start-container-delayed должен быть строго заблокирован,
разблокировать ТОЛЬКО после первого успешного старта"):

- Маска `halium-start-container-delayed.service`/`.timer` на `/dev/null`
  в `update-binary` ОСТАЁТСЯ как в v25/v26 -- на первой загрузке
  срабатывает СТРОГО ТОЛЬКО новый 45с-автостарт, ничего больше.
- `halium-first-boot-container-autostart.sh` (45с) в САМОМ КОНЦЕ своей
  работы -- уже ПОСЛЕ `touch CONTAINER_ENABLED` + `systemctl start
  lxc-android-config.service` -- сам снимает маску (`rm -f` обоих
  файлов). Поскольку `halium-restore-state.sh` уже отработал раньше в
  ЭТОЙ же загрузке (застал маску ещё на месте, его `cat >` ушёл в
  `/dev/null`, ничего не сгенерировал), реальное разблокирование
  вступает в силу только со **следующей** загрузки -- на ней
  `restore-state.sh` увидит путь уже свободным и сгенерирует юнит
  штатно.
- Задержка таймера для всех загрузок начиная со второй изменена с 15с
  (историческое значение) на **25с** (`halium-restore-state.sh`,
  heredoc `halium-start-container-delayed.timer`).
- Итог по загрузкам: 1-я -- только 45с-автостарт (delayed-механизм
  физически замаскирован, сработать не может в принципе); 2-я и все
  последующие -- обычный 25с-таймер (45с-юнит на них сам себя
  пропускает через `ConditionPathExists=!/userdata/CONTAINER_ENABLED`).

**НЕ протестировано живьём** -- нужен полный вайп Data + чистая
прошивка v27, чтобы честно проверить ОБА сценария разом (первая
загрузка через 45с, вторая/последующая через 25с).

## Что ещё осталось непроверенным / для следующей сессии
- Полная dpkg-консистентность после установки 3 новых .deb -- нужно
  живьём проверить `dpkg -l libhardware2 libhybris-common1
  pulseaudio-modules-droid-modern` после свежего флеша v27, убедиться
  что все три arm64-варианта теперь `ii`, и `pulseaudio-modules-droid-
  hidl` (тоже `iU` ранее) наконец сконфигурировался (`dpkg --configure -a`
  без остаточных ошибок).
- `Xwayland` бинарник отсутствует (`Cannot find Xwayland binary
  "/usr/bin/Xwayland"`) -- не критично для нативного Wayland-десктопа,
  но X11-приложения работать не будут. Не исследовано, куда делся
  пакет `xwayland` (возможно тот же класс arm64-пробела в бандле).
- `device-info: команда не найдена` в `/etc/profile.d/qtwebengine-gpu.sh`
  -- мелкая, не блокирующая ошибка, тот же симптом что уже
  задокументирован в `DROIDIAN-V25-PLAN.md` (задача 7, мобильная связь)
  для другого скрипта -- возможно, стоит поставить `device-info` одним
  фиксом сразу в обоих местах.
