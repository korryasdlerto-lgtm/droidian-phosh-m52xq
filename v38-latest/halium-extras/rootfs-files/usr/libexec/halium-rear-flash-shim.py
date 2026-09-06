#!/usr/bin/env python3
# 2026-09-02: заменяет настоящий /usr/libexec/flashlightd (тот
# управляет вспышкой через libhybris/gstreamer-droid -> Android
# CameraHAL -- падает, пока vendor.camera-provider-2-6 не поднят,
# что на этом устройстве не всегда получается из-за отсутствующих
# EFS-калибровок камеры).
#
# Найдено в параллельном Ubuntu Touch проекте на этом же устройстве
# (ФИКСЫ_ДЛЯ_В39.txt): реальный узел вспышки -- НЕ led:torch_0
# (leds-qti-flash, живьём подтверждено: запись принимается, света
# нет, в dmesg тишина), а Samsung-специфичный магический-значение
# узел /sys/devices/virtual/camera/flash/rear_flash, за которым
# стоит отдельная PMIC/charger-микросхема sm5714-fled. Протокол:
# 0=выкл, 100=torch on. Живьём на этом устройстве подтверждено:
# echo 100 > rear_flash -> полная цепочка в dmesg
# (sm5714_store -> fled_torch_enable -> sm5714_fled_control: Torch
# mode & used gpio(1)) И РЕАЛЬНЫЙ ФИЗИЧЕСКИЙ СВЕТ. Полностью в обход
# camera-provider/CamX.
#
# Права на rear_flash чинит отдельный halium-fix-torch-perm.sh
# (halium-fix-torch-perm.service, уже в multi-user.target.wants) --
# этот скрипт только реализует D-Bus интерфейс
# org.droidian.Flashlightd, который читает переключатель в шторке
# Phosh (интроспекция подтверждена живьём: метод
# SetBrightness(u bvalue), readwrite свойство Brightness).
import sys
from gi.repository import Gio, GLib

XML = """
<node>
  <interface name='org.droidian.Flashlightd'>
    <method name='SetBrightness'>
      <arg type='u' name='bvalue' direction='in'/>
    </method>
    <property name='Brightness' type='i' access='readwrite'/>
  </interface>
</node>
"""

REAR_FLASH = "/sys/devices/virtual/camera/flash/rear_flash"
PATH = "/org/droidian/Flashlightd"
IFACE = "org.droidian.Flashlightd"

brightness = 0
conn_ref = None


def write_flash(val):
    try:
        with open(REAR_FLASH, "w") as f:
            f.write("100" if val else "0")
    except Exception as e:
        print("write failed:", e, file=sys.stderr)


def emit_changed():
    if conn_ref is None:
        return
    conn_ref.emit_signal(
        None, PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged",
        GLib.Variant(
            "(sa{sv}as)",
            (IFACE, {"Brightness": GLib.Variant("i", brightness)}, []),
        ),
    )


def set_state(v):
    global brightness
    v = int(v)
    write_flash(v)
    brightness = v
    emit_changed()


def on_method_call(connection, sender, path, iface, method, params, invocation):
    if method == "SetBrightness":
        (bvalue,) = params.unpack()
        set_state(bvalue)
        invocation.return_value(None)
    else:
        invocation.return_error_literal(
            Gio.dbus_error_quark(), Gio.DBusError.UNKNOWN_METHOD, "no such method"
        )


def on_get_property(connection, sender, path, iface, prop):
    if prop == "Brightness":
        return GLib.Variant("i", brightness)
    return None


def on_set_property(connection, sender, path, iface, prop, value):
    if prop == "Brightness":
        set_state(value.unpack())
        return True
    return False


def on_bus_acquired(connection, name):
    global conn_ref
    conn_ref = connection
    node_info = Gio.DBusNodeInfo.new_for_xml(XML)
    connection.register_object(
        PATH,
        node_info.interfaces[0],
        on_method_call,
        on_get_property,
        on_set_property,
    )


def on_name_lost(connection, name):
    print("lost name, exiting", file=sys.stderr)
    sys.exit(1)


Gio.bus_own_name(
    Gio.BusType.SESSION,
    "org.droidian.Flashlightd",
    Gio.BusNameOwnerFlags.REPLACE | Gio.BusNameOwnerFlags.ALLOW_REPLACEMENT,
    on_bus_acquired,
    None,
    on_name_lost,
)

GLib.MainLoop().run()
