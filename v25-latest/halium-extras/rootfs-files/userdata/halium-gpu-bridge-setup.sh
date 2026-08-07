#!/bin/sh
# Bind host /vendor, /system, /dev/__properties__ onto the running Android
# container's real filesystem, and swap in the hwcomposer-capable phoc build,
# so phoc/libhybris (running on the host/glibc side) can find EGL/GLES vendor
# libraries and read real Android system properties (ro.hardware.egl etc).
#
# /vendor and /system must already exist as empty directories in the host
# rootfs.img (created once via: mount -o remount,rw /; mkdir -p /vendor
# /system; mount -o remount,ro /) -- ExecStartPre can bind-mount onto them
# but can't create them on a read-only root.

# FOUND 2026-07-21 live: the old /userdata/phoc-downgrade-backup/phoc-0.47.0-old
# copy lost its executable bit at some point (plain `cp`, never `chmod +x`'d),
# so this bind-mount silently succeeded but phosh.service crash-looped with
# "dbus-run-session: failed to exec '/usr/bin/phoc': Permission denied".
# Prefer the rootfs.img-shipped copy (usr/bin/phoc-0.47.0, ships with correct
# perms on every flash) and fall back to the old /userdata path for
# already-flashed devices that only have that one; chmod defensively either way.
#
# FOUND AGAIN 2026-07-21, later same day: the version-string idempotency
# check below ("already 0.47.0, skip re-mount") is not enough on its own --
# seen live losing the +x bit again on a LATER phosh.service restart even
# after a prior restart had it working, root cause not fully pinned down
# (suspected race across the rapid ExecStartPre-triggered restart cycle).
# Make the chmod itself unconditional, every single run, regardless of the
# version check outcome or whether the bind-mount already happened -- it's
# a no-op if already correct, cheap, and removes this whole class of bug.
chmod 755 /usr/bin/phoc 2>/dev/null || true
if ! /usr/bin/phoc --version 2>/dev/null | grep -q "0.47.0"; then
    if [ -f /usr/bin/phoc-0.47.0 ]; then
        chmod 755 /usr/bin/phoc-0.47.0 2>/dev/null || true
        mount --bind /usr/bin/phoc-0.47.0 /usr/bin/phoc 2>/dev/null || true
    elif [ -f /userdata/phoc-downgrade-backup/phoc-0.47.0-old ]; then
        chmod 755 /userdata/phoc-downgrade-backup/phoc-0.47.0-old 2>/dev/null || true
        mount --bind /userdata/phoc-downgrade-backup/phoc-0.47.0-old /usr/bin/phoc 2>/dev/null || true
    fi
    chmod 755 /usr/bin/phoc 2>/dev/null || true
fi

mountpoint -q /vendor || mount --bind /android/vendor /vendor 2>/dev/null || true
mountpoint -q /system || mount --bind /android/system/system /system 2>/dev/null || true

CPID=$(lxc-info -n android 2>/dev/null | awk '/^PID:/{print $2}')
if [ -n "$CPID" ] && [ -d "/proc/$CPID/root/dev/__properties__" ]; then
    mount --bind "/proc/$CPID/root/dev/__properties__" /dev/__properties__ 2>/dev/null || true
fi

if [ -x /userdata/phosh-session-debug-clean ]; then
    mountpoint -q /usr/bin/phosh-session || mount --bind /userdata/phosh-session-debug-clean /usr/bin/phosh-session 2>/dev/null || true
fi

exit 0
