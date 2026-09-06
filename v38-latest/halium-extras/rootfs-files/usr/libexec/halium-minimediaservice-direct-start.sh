#!/bin/sh
# See halium-minimediaservice-direct-start.service for the full story --
# Android init's own auto-respawn for this service is broken on this
# patched init, so systemd supervises it directly instead (Restart=always
# in the unit), same bypass pattern as halium-camera-provider-direct-
# start.sh.
#
# CLAUDE_DEBUG 2026-09-05: same lxc-attach/namespace pitfall as the
# camera provider (see its script) -- `systemctl restart` only kills the
# outer lxc-attach wrapper, not necessarily the process inside the
# container's own namespace. Pre-kill defensively before every exec.
lxc-attach -n android -- pkill -9 -x minimediaservice 2>/dev/null || true
sleep 1
exec lxc-attach -n android -- /system/bin/minimediaservice
