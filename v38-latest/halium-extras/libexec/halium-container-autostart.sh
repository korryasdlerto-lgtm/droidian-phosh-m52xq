#!/bin/sh
# Единый постоянный механизм автостарта Android-контейнера -- работает
# на КАЖДОЙ загрузке (не только первой), через таймер с задержкой 25с
# (halium-container-autostart.timer). Разница между первым и всеми
# последующими стартами -- только в задержке, реализована ВНУТРИ этого
# скрипта, без отдельной маскировки/размаскировки других юнитов:
#   - маркера /userdata/CONTAINER_ENABLED ещё нет -> это первый запуск
#     после флеша, досыпаем ещё 20с (итого 45с с момента boot, вместо
#     обычных 25с) -- даём хост-стороне (systemd/Phosh/usb-moded)
#     побольше времени стабилизироваться на самой нестабильной
#     загрузке, ПОТОМ создаём маркер и стартуем.
#   - маркер уже есть -> обычная загрузка, стартуем сразу (25с с
#     момента boot, без дополнительной задержки).
LOG_TAG="halium-container-autostart"
log() {
    echo "$LOG_TAG: $1" | systemd-cat -t "$LOG_TAG" -p info
}

if [ ! -f /userdata/CONTAINER_ENABLED ]; then
    log "no marker yet -- first boot, sleeping extra 20s (45s total)"
    sleep 20
    touch /userdata/CONTAINER_ENABLED
    log "marker created"
else
    log "marker already exists -- regular boot (25s total)"
fi

systemctl start lxc-android-config.service
log "container start requested"

# НАЙДЕНО В ПАРАЛЛЕЛЬНОМ (Ubuntu Touch) ПРОЕКТЕ НА ЭТОМ ЖЕ УСТРОЙСТВЕ,
# ПОРТИРОВАНО СЮДА 2026-09-01: system.img -- ТОТ ЖЕ САМЫЙ файл, что и в
# Ubuntu Touch-проекте (общий симлинк), userdebug-сборка. RescueParty.java
# (isDisabled(), ~строка 151) содержит:
#   if (Build.IS_USERDEBUG && isUsbActive()) { return true; /* disabled */ }
# -- на подключённом USB RescueParty САМООТКЛЮЧАЕТСЯ, но на батарее
# (USB неактивен) защита снимается: при повторяющемся краше system_server
# (например, NullPointerException от ConnectivityManager, т.к. tethering/
# resolv APEX намеренно скрыты) RescueParty проходит полную эскалационную
# лестницу (RESET_SETTINGS_* -> WARM_REBOOT) за ~45 секунд и финальный
# system_server шлёт sys.powerctl='reboot,RescueParty' -- НАСТОЯЩИЙ ПОЛНЫЙ
# РЕБУТ УСТРОЙСТВА, инициированный самим Android, не краш "снизу". В
# Ubuntu Touch-проекте это объяснило "выключение на батарее ~5-8 мин",
# долго списывавшееся на другие причины. persist.sys.disable_rescue=true
# -- официальное AOSP-свойство, полностью отключает RescueParty,
# переживает перезагрузку (хранится в
# /data/property/persistent_properties внутри контейнера). К этому
# моменту lxc-android-ready (ExecStartPost) уже отработал -- systemctl
# start синхронный и не возвращается, пока весь ExecStartPost не
# завершится -- property-сервис контейнера точно поднят.
lxc-attach -n android -- setprop persist.sys.disable_rescue true 2>&1 | \
    while IFS= read -r line; do log "disable_rescue setprop: $line"; done
log "persist.sys.disable_rescue=true applied"
