# droidian-phosh-m52xq

Droidian (Debian + `phosh`) port for the **Samsung Galaxy M52 5G** (codename
`m52xq`, models `SM-M526B`/`SM-M526BR`), using the device's original Android
vendor kernel and HAL blobs bridged into a real Debian/systemd userspace via
`libhybris` — the same underlying bridging technique as Halium, but using
Droidian's own packaging/build conventions instead of a from-scratch Halium
image. This is a **separate, parallel port** from the
[Halium 13 + Plasma Mobile effort](https://github.com/korryasdlerto-lgtm/halium13-plasma-mobile-m52xq)
for the same device — both exist because they hit different bugs at
different points, and cross-checking one against the other has repeatedly
been useful (see the `ofonod-wrapper`/binder-plugin note in
[v11/v12](#v11v12-same-lineage-fstab-root-cause-mobile-data-audio-userspace-polkit-final-fix)
below, which reuses a fix originally found on the Ubuntu Touch/Halium side
of this device's work).

**Current status: real Adreno GPU-accelerated rendering (via hwcomposer),
working touch input, survives a cold reboot, working SSH, and (as of v37)
a source-level root-cause fix to a `libhybris` EGL bug that was silently
zeroing out every usable GPU display configuration on the Wayland
platform (see [v36/v37](#v36v37-the-real-egl-root-cause-fix-not-a-workaround)
below — this is the project's deepest fix so far: found by reading the
vendored `libhybris` fork's own source, not by trial and error). Screen
lock now genuinely requires and accepts a PIN again after being
accidentally short-circuited for months (v37/v38). Audio has gone through
a full stack rebuild (PipeWire → PulseAudio+armhf bridge) and is believed
complete but not yet flash-tested. Container auto-start was abandoned
after three failed automated approaches — it is now intentionally
manual-only. The camera viewfinder is still black — the EGL fix was
necessary but not sufficient; the actual working rendering path
(CPU-side NV21 callback frames, bypassing GL textures) exists only
compiled into an older binary and its source was lost in an earlier
fork/rebase (see [v38 (cont'd)](#v38-contd-camera-architecture-work-and-the-lost-nv21-callback-source)).
Telephony (modem/RIL) is actively broken. The Phosh↔SurfaceFlinger
compositor hand-off is a real, still-unsolved hardware-composer-client
race under active investigation (see
[v38](#v38-phocsurfaceflinger-composer-client-race)). Suspend-to-RAM does
not actually suspend (screen blanks, CPU stays awake) — diagnosed but not
fixed. This is an honest, warts-and-all snapshot of a long-running,
iterative debugging project (38 versioned iterations so far), not a
finished ROM.**

## What this actually is

Same bridging concept as the Halium port: the phone's original, unmodified
Android vendor kernel and proprietary HAL blobs (GPU driver, WiFi firmware,
modem RIL, sensors, etc) keep running inside an **LXC container** whose
only job is to host those HAL services — there is no visible Android UI.
`libhybris` translates between Android's bionic-based HAL libraries and the
host's glibc-based Debian userspace. On top of that bridge, this port runs
Droidian's own package set (`phosh` as the mobile shell, `pulseaudio` with
the `droid` HAL modules for audio, `ofono` with the Android `binder` RIL
plugin for telephony) instead of a custom-built Halium image.

## Repository layout

- **`v38-latest/`** — snapshot of the most recent versioned working
  folder (`v38`, chronologically newest of the `v1`..`v38` iteration
  history kept locally), synced with everything **except proprietary
  Samsung/Qualcomm vendor blobs** (see `PROPRIETARY-FILES.txt`) **and
  files over 100MB** that aren't project-authored (rootfs/userdata
  image chunks, the vendor kernel-modules tarball).
  - `v38-latest/firmware/boot.img`, `vendor_boot.img` — real file
    content (not the symlinks the working folder normally uses, which
    point outside the tracked tree to shared build outputs) for the
    exact boot/vendor_boot pair this version is flashed and tested
    with.
  - `v38-latest/firmware/system.img`, `v38-latest/data/rootfs-chunks`,
    `v38-latest/data/userdata-overlay.tar.gz` — **symlinks**, not
    included in git (each would be gigabytes and none is
    project-authored content). They point at
    `../../local-large-files/...`, i.e. a sibling folder next to
    `v38-latest/` at the repo root that isn't tracked by git. To build
    a real flashable ZIP: create `local-large-files/` next to
    `v38-latest/`, drop your own `system.img` (extracted Android
    system partition), `rootfs-chunks/` (the `rootfs.img.part-N` files
    produced by `split -b`, see [Flashing](#flashing)) and
    `userdata-overlay.tar.gz` in it, and the packaging step below
    picks them up through the symlinks automatically — no editing of
    anything under `v38-latest/` required.
  - `v38-latest/mount-patched-v3.sh` — the actual boot-time mount/init
    script flashed to the device for this version.
  - `v38-latest/halium-extras/` — the bridge scripts, systemd units,
    and package overlay (`rootfs-files/`) applied on top of the base
    Droidian rootfs.
  - `v38-latest/META-INF/` — the TWRP-flashable ZIP installer scripts.
  - `v38-latest/FIXES-*.md`, `ФИКСЫ_ДЛЯ_В36.txt` — this version's own
    changelog/investigation notes.
- **`kernel-config/lineage-m52xq_defconfig`** — the kernel `.config`
  currently in use, kept at the repo root since it applies across
  versions (see [Recurring themes](#recurring-themes) item 7 and the
  v10/v25 kernel-config history entries for why specific options in it
  matter).
- **`PROPRIETARY-FILES.txt`** — flat list of the Samsung/Qualcomm/
  ArcSoft vendor blob paths deliberately excluded from this repo, with
  instructions for extracting them from your own device's stock
  firmware (same convention LineageOS/Halium device trees use for
  their own `proprietary-files.txt`).

## Full project history (v1 → v38)

Each numbered version represents one real flash-and-test iteration on
physical hardware. This section is the project's lab notebook,
condensed from the original per-version `.md`/`.txt` files (`DROIDIAN-
vN-PLAN.md`, `DROIDIAN-vN-SYSTEMD-FIX-HOWTO.md`, `FIXES-vN-*.md`,
`ФИКСЫ_ДЛЯ_В36.txt` — only `v38`'s own copies are kept in full under
`v38-latest/`, the rest exist only in this history section). Kept in
full technical detail deliberately — exact paths, service names, error
strings — since this is meant to be reference material for whoever
debugs the next regression, not a marketing changelog.

### v1 → v2: the foundational bootloop (systemd + kernel `close_range()`)

- **v1 symptom**: flashed device boot-looped at ~103s. Diagnosed via
  `adb shell "cat /sys/fs/pstore/console-ramoops-0"` while sitting in
  TWRP post-rollback — every systemd unit failed identically with
  `status=202/FDS` (e.g. `cron.service: Failed to close unwanted file
  descriptors: Invalid argument`).
- **Root cause**: installing `phoc`/`phosh` from Droidian's rolling
  repo silently pulled systemd 252→257 (git snapshot). systemd ≥247
  depends on `close_range()`, which the vendor kernel (5.4.147/233)
  doesn't implement — every service failed to spawn, including the
  udev firmware loader for IPA, so the kernel hung in
  `request_firmware()`/`pil_boot` until the hardware watchdog rebooted
  the device.
- Downgrading systemd via apt pinning alone (`/etc/apt/preferences.d/
  99-pin-systemd-bookworm`) fixed the dependency cascade but **v2
  still boot-looped identically** — proved the syscall itself, not
  just the systemd version, was the real blocker.
- **Real fix**: backported `close_range()` into an isolated copy of
  the kernel tree (`~/kernel-droidian-experiment/sm7325`, never
  touching the flashed Ubuntu Touch kernel). Patched 7 files,
  renumbered vendor's misplaced `process_madvise` from syscall #436 to
  upstream #440, added `close_range` at #436 (flags=0 only). Repacked
  as `droidian-boot-v3-writablesystem.img`. **Confirmed working**:
  500+s survival vs. the previous 103s crash. Open risk (never fully
  resolved): if vendor code calls `process_madvise` by the old #436,
  it now hits `close_range` instead — never triggered in practice.
- Side fixes same session: `openssl-provider-legacy`/`libssl3` dpkg
  unpack race; `dnsmasq-base` (`u!` sysusers.d modifier incompatible
  with old systemd).
- `lxc-android-config` was never installed as a package (only loose
  files existed) — installed from `repo.ubports.com`; Android AID
  groups (bluetooth/radio/gps/android_net*/media/etc.) added to
  `phablet`.
- `lxc`/`liblxc1` 6.0.6 (Droidian rolling) incompatible with
  `lxc-android-config` (built for ~5.0.x) — pinned to bookworm
  `5.0.2-1+deb12u4`; `halium-kickstart-lxc` dropped from 143s to ~20s.
- `/android` tmpfs stuck at 4KB (size-expansion code gated on a
  condition that was always false on this device) → ENOSPC — fixed
  with unconditional `mount -o remount,size=67108864 /android`.
- `phoc` SIGSEGV: missing unversioned `libEGL.so`/`libGLESv2.so`
  (GLVND dispatch); later found mixing vendor (`_adreno`) and
  framework EGL/GLES libs broke an internal trampoline symbol — fixed
  by using the **framework** `libEGL.so` pulled from `/system/lib64`,
  placed in `system-lib64` not `vendor-lib64`.
- **ELOOP root-caused**: `${R}/system/bin -> /system/bin`
  self-referential symlink (container root *is* the system partition)
  broke `exec("/init")`. Traced further to `/system` being
  force-mounted `ro` in initrd's `scripts/halium` (hardcoded, ignoring
  the `.writable_image` marker) — fixed by patching that branch to
  `-o $MOUNT`.
- **`sync_dirs()` "trust problem" root-caused**: the Ubuntu-Touch-style
  writable-paths merge only copies rootfs.img files into `/userdata/
  system-data/<path>` if they don't already exist there — never
  updates. Explains months of "disk shows the fix, live system
  doesn't" confusion (recurs — see [Recurring
  themes](#recurring-themes) below).
- PropertyInit SIGSEGV chain: `ContextsSerialized::
  GetPropAreaForName()` NULL-deref in bootstrap `libc.so`,
  binary-patched (1 instruction); real cause was
  `CreateSerializedPropertyInfo()` non-idempotence — fixed via `rm -rf
  /dev/__properties__/*` in `pre-start.sh`.
- `/apex` missing (flattened APEX, double-nested `system/system/
  apex/...`) — added dedicated tmpfs mountpoint + 4 targeted binds
  (runtime/art/i18n/vndk.current); deliberately excluded
  `com.android.tethering`/`com.android.resolv` (known SSH-killer,
  cross-referenced from the parallel Ubuntu Touch project).
- 4 binary patches to `libselinux.so` (`selinux_check_access`
  @0xf678, `selinux_android_setcontext` @0x14df8,
  `selinux_android_restorecon`/`_pkgdir` @0x1573c/0x160bc), all
  `mov w0,#0; ret` — unblocked coldboot wait, zygote registration,
  installd. **Critical safety finding**: loading a *real* compiled
  sepolicy into `/sys/fs/selinux/load` crashed/rebooted the whole
  device even in permissive mode — never do this again.
- Native DRM/pixman bypass attempt (skip hwcomposer entirely) hit a
  NULL deref inside the proprietary Qualcomm SDE RSC driver on cold
  atomic commit — concluded native DRM is a dead end on this SoC; must
  go through the `hwcomposer` backend.
- Milestone: `system_server` fully completes `SystemServer.run()`,
  `PackageManagerService` works, `installd` processes ~90 packages,
  several LineageOS-specific NPEs fixed via smali/baksmali patches to
  `services.jar`/`framework.jar`/`org.lineageos.platform.jar`.
  `sys.boot_completed` still never set at this point.

### v3/v4: packaging pivot + host-side desktop breakthrough begins

- TWRP's `unzip -p` (toybox/busybox) has a hard **4GiB boundary bug**
  — rootfs.img (5.38GB) failed with `scudo: internal map failure`;
  fixed by chunking with `split -b`.
- **Architecture pivot**: stopped growing rootfs.img via `resize2fs`;
  instead bind-mount `/home` from `/userdata/phosh-home-data`
  (`home.mount` unit).
- TWRP's `ash`/toybox **cannot reliably write heredocs** (`<<EOF`) —
  silently produces empty files (`home.mount` shipped as 0 bytes).
  Project-wide lesson: always use `printf '%s\n' ... > file` instead.
- `mount.sh` (the real 87KB boot hook) was meant to be
  runtime-bind-mounted over the stock 686-byte stub via
  `halium-mount-patch.service` — unreliable; fixed by baking the
  patched file directly into `rootfs-files`.
- `cp -a` "Too many symbolic links" during injection: fixed by moving
  `rootfs-files/lib/*` into `usr/lib/*` (target `/lib` is a merged-usr
  symlink).
- Zygote/system_server crash chain fully dispatched via cascading
  fixes: missing `ANDROID_ROOT`/`ANDROID_DATA`/etc. env vars,
  SHALLOWFIX list expanded (framework/app/priv-app/fonts/usr), broken
  `build.prop` symlink replaced with real bind-mount, missing
  `/mnt/user/0`/`/storage`, second `selinux_android_setcontext` patch,
  `libnativeloader.so` copy fix (later obsoleted by working
  linkerconfig), `/data/data` self-inflicted symlink causing EXDEV in
  installd, LineageOS `LongScreen`/`LineageSettings` NPE fixed via
  smali patch to `org.lineageos.platform.jar`. Result: `system_server`
  no longer crash-loops for the first time in the project.
- Extended chain of `ConnectivityManager`/netd null-check patches to
  `services.jar` (final `services-patched13.jar`) — reached
  `OnBootPhase_600`.
- `com.android.phone` crash-loop (startup race, `SettingsProvider` not
  ready) fixed via null-checks in `framework.jar`'s
  `Settings$NameValueCache`.
- Conclusion at this point: `SystemServer.run()` fully completes and
  goes idle — remaining blocker to `boot_completed`/screen-on judged
  to be the SurfaceFlinger↔hwcomposer HAL pipeline, not further Java
  patching.

### v5/v6: SSH/USB marathon + host-side hwcomposer desktop confirmed working

- Long root-cause chain for SSH/USB (each a real, separate bug):
  `usb-rescue-mode-off.service` killing rescue mode on Phosh start;
  `usb_moded` reading only `/etc/usb-moded/*.ini` not rendered
  templates; `/etc` read-only blocking renders (overlayfs gave EPERM
  on this kernel — switched to tmpfs+bind); `developer_mode` rejected
  as default for uid 0; D-Bus `set_mode` not rebuilding a live gadget;
  **`openssh-server` was never installed at all** (final root cause of
  "Connection refused"); missing `gettext-base`/`envsubst`; missing
  `usb-moded-ubports-config` package assets; a vendor unit
  `lxc-android-config-disable-ssh-socket.service` silently disabling
  `ssh.socket` every boot; RNDIS mode flapping every ~3s — root-caused
  via reading actual `sailfishos/usb-moded` source: `network=1` in
  `developer_mode.ini.in` triggers a failing retry that rolls back the
  mode — fixed with `network=0; appsync=0; dhcp_server=0`.
- **Recurring systemic bug**: `lxc.environment PATH=` (meant for the
  guest) leaked into the **host** `mount.sh` hook, silently breaking
  `install`/`mount`/`grep`/`head` for an unknown span of project
  history — fixed by explicitly resetting `PATH` at the top of
  `mount.sh` (recurs — see [Recurring themes](#recurring-themes)).
- First fully clean flash of v5 exposed 3 new bugs never seen on a
  "warm" (already-patched) device: SELinux-patch unit's enable symlink
  pointed at the wrong path convention; SSH credentials
  (`PermitEmptyPasswords`, password hashes) were only ever patched
  live, never committed to source; `/etc/systemd/system` is a
  bind-mount over `/userdata/system-data/...` so partial edits don't
  survive.
- `phoc` repeatedly lost its `+x` bit (idempotency check bug in
  `halium-gpu-bridge-setup.sh`) — fixed with unconditional `chmod
  755`.
- **Milestone**: `phosh.service` reached `active (running)` stably
  (13+ min, real touch via libinput) **and** in-container
  `sys.boot_completed=1` simultaneously for the first time — causal
  link between the two never established.
- v6-only new findings: `/vendor` intermittently hangs under heavy I/O
  (unresolved); `hwservicemanager` spam for `media.c2::
  IComponentStore` (new, possible overheat contributor);
  `halium-early-remount-rw.service` caused one full boot failure and
  was rolled back without full diagnosis; automating the touch fix
  (`rmmod`/`insmod synaptics_ts`) via `ExecStartPre=` caused a full
  device hang and was reverted.

### v7: real Phosh desktop with touch achieved

- **Three stacked races blocking Phosh rendering solved**:
  `XDG_RUNTIME_DIR` not inherited, `gnome-session` racing logind for
  session-type detection, `phosh-session`'s isolated D-Bus session
  lacking `org.freedesktop.systemd1`. Fixed with one drop-in forcing
  `XDG_SESSION_TYPE=wayland` + `DBUS_SESSION_BUS_ADDRESS=unix:path=
  /run/user/1000/bus`. **Confirmed across two reboots** — real Phosh
  visible, not just Samsung splash.
- Touch fix (`rmmod`/`insmod synaptics_ts`) confirmed manually but
  automating via `ExecStartPre=` caused a device hang — kept
  manual-only.
- Unresolved at v7 end: Settings button does nothing; torch toggle
  does nothing (hardware path works via direct sysfs write);
  `system_server` still periodically SIGSEGVs/restarts zygote even
  with Phosh up; `hwservicemanager` media.c2 spam.

### v8: network-vs-container race mitigation

- First clean v7 flash installed fine, but next boot had no SSH for a
  long time — traced to the (still-unsolved) zygote/system_server
  restart-loop starving early-boot SSH/network of CPU/IO.
- Fix: `halium-wait-network-before-lxc.sh` delays container start
  until `usb0` is stable 20s (cap 90s), wired via `ExecStartPre=` on
  `lxc-android-config.service`.
- Found and fixed a **`/data` vs `/userdata` path-naming trap**: same
  partition is `/data` under TWRP but `/userdata` under the booted
  OS — a script referenced by the wrong path caused
  `status=203/EXEC`.

### v9: real usb_moded root cause found

- `usb_moded` was crash-looping with `library "libcutils.so" not
  found` — `HYBRIS_LD_LIBRARY_PATH` was wired into every other
  bridge-dependent service except `usb-moded.service`. Fixed via a new
  drop-in.
- Structural fix replacing the earlier "re-send rescue-off signal"
  patch: `USB_MODED_ARGS=` (empty) so `usb_moded` never enters rescue
  mode at all.
- Simplified the network-wait logic to a flat `sleep 70; exit 0`.
- **Confirmed**: first fully successful cold boot of the whole
  session — SD card, SSH, container, and phosh all `active` with zero
  manual intervention.
- Unresolved: Settings button, torch toggle, periodic zygote
  SIGSEGV/restart, `usb_moded`'s underlying instability never
  explained (only worked around).

### v10: overlayfs for full `/data` capacity + audio kernel fix + persistence mechanism

- **Solved**: apt installs were filling the tiny (~5GB) rootfs.img
  instead of the 98GB `/data` partition. Found that a full-root
  overlayfs mechanism already existed in initrd (`scripts/halium`) but
  was never activated — needed **both** `.writable_image` and
  `.writable_image_overlay` markers together (one alone gets
  remounted back to `ro` by an `else` branch).
- **Audio root cause found**: `insmod apr_dlkm.ko` failed with
  `exports duplicate symbol apr_send_pkt (owned by kernel)` — the
  defconfig had `CONFIG_QCOM_APR=y` bundled in with the
  modem-boot-critical `CONFIG_MSM_PIL_MSS_QDSP6V5=y`/
  `CONFIG_SND_SOC_QCOM=y`, but Kconfig dependency analysis proved APR
  has no dependency relation to the modem fix — it was purely an
  accidental regression. **Fix**: `# CONFIG_QCOM_APR is not set` in
  `lineage-m52xq_defconfig` (this Droidian kernel tree only, not the
  separate Ubuntu Touch/Halium tree). Rebuilt kernel had zero
  `apr_send_pkt` errors. (Audio itself keeps breaking again at
  higher levels — see [Recurring themes](#recurring-themes).)
- Live persistence mechanism built: `halium-save-state.sh`/
  `halium-restore-state.sh` for WiFi/Bluetooth/time/locale/brightness/
  clock — clock restore had a bug (`save` wrote, `restore` never read)
  fixed same session; added an automatic NTP-less clock fix via
  `wget --no-check-certificate` HTTPS `Date:` header parsing in the
  background.
- polkit "password prompt on reboot" investigated: found
  `/etc/polkit-1/rules.d` had `700` perms (polkitd couldn't even open
  the directory), fixed to `755`, but UI still prompted — real root
  cause found in v11/v12 (see below).
- Russian locale (`ru_RU.UTF-8`) installed live.

### v11/v12 (same lineage): fstab root cause, mobile data, audio userspace, polkit final fix

- **Major finding**: `/etc/fstab` is not a static file — it's a
  bind-mount onto `/run/image.fstab`, generated fresh every boot by
  `process_bind_mounts()` in initrd, which unconditionally writes
  `/dev/root / rootfs defaults,ro 0 0` as its first line, before any
  writable-paths logic runs. This explained why two separate v10 fix
  attempts had both been no-ops. **Real fix**: patch the `ro`→`rw`
  string directly in the initrd script itself, rebuilt as
  `droidian-boot-v5-fstabfix.img`.
- Combined with removing an unnecessary `journald` dependency
  (`00-wait-for-rw.conf` required a service that could hang, blocking
  nearly all of systemd behind `journald.socket`) and an 80s
  container-start delay: **first true clean-flash boot to a working
  Phosh desktop**, confirmed live.
- **Mobile data (ofono)**: two real bugs found —
  `device-info` utility missing broke `ofonod-wrapper`'s ril/binder
  plugin selection (same *family* of bug as the already-solved Ubuntu
  Touch "oFono binder plugin" issue, different trigger); missing
  `radio` system user (UID 1001) for Android AID compat. Both fixed
  but **still 0 modems reported at v11's end** — still unresolved as
  of v25 (see [Known unresolved](#known-unresolved-telephony-rilmodem)
  below).
- **Audio userspace**: kernel confirmed 100% fixed (real ALSA card
  visible, all DSP modules loaded), but PipeWire/WirePlumber couldn't
  route the card (no ALSA UCM2 profile for this Android/LPASS
  architecture; direct `amixer`/`aplay` also failed). Found that audio
  *had* worked earlier via `pulseaudio-modules-droid-24` + a
  HIDL-compat bind-mount, but that package had disappeared from the
  install list when the stack switched to PipeWire, and reinstalling
  conflicted with the currently-load-bearing `libhybris-common1` fork
  (`lindroid.drm`) that GPU/touch depend on — deliberately not forced,
  deferred (this becomes the entire subject of v21).
- **polkit reboot-password prompt solved**: real action id caught via
  `busctl monitor` was `org.freedesktop.login1.reboot-ignore-inhibit`
  (not plain `.reboot`); also found `polkit-agent-helper-1` lacked
  `setuid root` entirely, so no password would ever have worked. Both
  fixed, confirmed live.
- **SSH/systemd totally dead on clean flash, 4 stacked bugs**: (1)
  `/init`'s "already configured" guard (`[ ! -e /proc/self/exe ]`) was
  always false due to ambient `/proc` visibility, so
  `mount_userdata()`/fstab generation was skipped every boot — fixed
  with a self-created marker file; (2) consequence of bug 1: `/dev/
  null` mounted `root:root 0600` (classic devtmpfs/udev race, no
  udevd running yet) — fixed with direct `chmod 666` in `/init`; (3)
  `radio` user still missing from `/etc/passwd` after a data wipe,
  breaking `dbus-daemon`'s entire config parse (not just ofono); (4)
  `AmbientCapabilities=CAP_AUDIT_WRITE` in `dbus.service` failed with
  `EPERM` on this kernel (matches a known Docker-container issue,
  Red Hat Bugzilla #1115533) — fixed with a drop-in clearing it. Also
  trimmed `lsm=` cmdline (dropped smack/tomoyo/integrity/safesetid).
- **SSH death from a bad `liblxc` downgrade + full container gating**:
  `lxc-start` looped on `Unsupported config key "lxc.seccomp"` even
  though the config never mentions it — root cause was `lxc`/
  `liblxc1` having silently regressed from working `6.0.6-3` to
  bookworm-security `5.0.2-1+deb12u4`, which can't resolve the default
  seccomp profile. Fixed by reinstalling `6.0.6-3`, and by decoupling
  `lxc-android-config.service` from `sysinit.target` entirely (it
  previously blocked *all* subsequent boot behind its own retry
  storm).
- **Zygote "no namespace called com_android_art"**: the existing
  linkerconfig-visibility fix only patched the top-level
  `ld.config.txt`; the ART APEX has its *own* nested
  `com.android.art/ld.config.txt` where the namespace is called
  `default`, not `com_android_art`, and had never been patched. Added
  a second `sed`. Confirmed zygote stabilizes.
- **Three independent, silently-conflicting file-injection paths** in
  `update-binary` (rootfs-chunks master image, `halium-extras/
  rootfs-files/` overlay-copy, `halium-extras/units/` hardcoded enable
  list, plus a stale `userdata-overlay.tar.gz` snapshot unpacked in
  the wrong order) caused live-verified fixes to keep "reverting" on
  fresh flashes. Established a standing rule: always check all
  three/four locations, not just the master image.
- `Wants=` back-reference to `usb-moded-ssh.service` (an atypically-
  early rescue unit) unreliably failed to pull in
  `lxc-android-config.service` because the target could already be
  active before the drop-in was loaded — fixed with an explicit
  `multi-user.target.wants` symlink instead.
- **SystemServer crash-loop on LineageOS `LongScreen`/
  `LineageSettings` NPE** (a startup race in
  `installSystemProviders()`): fixed via a smali bytecode patch
  (`org.lineageos.platform.jar`, unconditional early `return-void` in
  `LongScreen.<init>`). Two reusable gotchas documented: (a) `zip -j`
  update-mode silently fails to update `classes.dex` in this jar's
  non-standard `soong_zip` structure — must fully unzip/replace/rezip;
  (b) must also delete the `.odex`/`.vdex` AOT cache or ART keeps
  running the old bytecode.
- **Months-long "phoc loses seat after ~20s" bug, partially solved**:
  extensive elimination (PAM session class, power-button timing, a
  broken/no-op watchdog script, idle timers, camera-watch script) all
  ruled out. Real fix: `seatd` was never installed, so `phoc` fell
  through to the buggy `logind` backend. Installing `seatd` made
  `phoc` use the `seatd` backend instead — **confirmed stable for 60+
  minutes**. Underlying logind/PAM seat-assignment bug itself never
  found. Side regression: Phosh's brightness slider (calls
  `login1.Session.SetBrightness` directly, bypassing libseat) broke
  under `seatd` since there's no logind seat anymore — sysfs write
  confirmed as a manual workaround. (Real `seatd` package files
  committed to `rootfs-files` in v18.)

### v21: PulseAudio (not PipeWire) + 32-bit armhf audio bridge

- **Root cause of no audio at all**: the real Qualcomm audio HAL
  (`audio.primary.lahaina.so`) only exists as a **32-bit** build; the
  64-bit `/vendor/lib64/hw/audio.primary.default.so` is an empty AOSP
  stub that segfaults.
- **Fix**: full droidian-native PulseAudio (not PipeWire) +
  `pulseaudio-modules-droid-modern` in **armhf**, running via Debian
  multiarch alongside the arm64 host. Removed `pipewire-audio`/
  `pipewire-alsa` (apt conflict); added ~292 armhf/arm64 `.deb`s
  bundled under `halium-extras/rootfs-files/usr/local/lib/
  halium-audio-fix-debs/`.
- Technical specifics: `HYBRIS_LD_LIBRARY_PATH` for the 32-bit process
  must point at `/system/lib/bootstrap:/system/lib:/vendor/lib` (not
  the 64-bit bridge dirs); `pulseaudio.service`'s default
  `SystemCallArchitectures=native` kills 32-bit ARM processes on
  aarch64 with instant SIGSYS — cleared via drop-in along with other
  systemd sandboxing (`NoNewPrivileges`, `LockPersonality`,
  `MemoryDenyWriteExecute`, `RestrictNamespaces`).
- **Confirmed working**: a 440Hz test tone was audible;
  `pulseaudio.service` stable under normal systemd management.
- **Post-flash disaster**: installing ~290 packages via `dpkg -i` on
  an **already-booted overlayfs system** corrupted ~50 files into
  overlayfs whiteout character-device stubs, causing a bootloop.
  **Fixed structurally**: package installation moved into
  `update-binary`, chrooted directly into rootfs.img **during
  flashing**, before the overlay exists at all (plain ext4 at that
  point) — completely removes overlayfs from the equation for this
  step.
- Additional bugs found/fixed same session: `pulseaudio.service`'s
  `Type=notify` never gets `sd_notify` from this build → systemd waits
  90s then kills an already-working process (fixed to `Type=simple`);
  `pipewire-pulse.socket` (not removed, only `-audio`/`-alsa` were)
  wins the race for the same native socket path — masked and
  `pulseaudio.socket` explicitly enabled; `gnome-control-center`/
  `gnome-settings-daemon` depend on pipewire-audio, requiring explicit
  removal+reinstall ordering in the chroot script.

### v22: first-boot container gate + battery/overheat investigation

- First live v21 flash hit two bugs: (1) chroot install failed with
  `dpkg: not found` (exit 127) — TWRP's shell `PATH=/sbin:/system/bin`
  lacks `/usr/bin`; fixed with explicit `export PATH=...` inside the
  chroot; (2) first boot after flash hung entirely (no USB) — pstore
  showed no panic, only expected `_HYBRIS_DISABLED` noise, so root
  cause was resource contention from the Android container starting
  too early, not a crash.
- **Container gate v1**: a `PHOSH_FIRST_BOOT_OK` boolean gate — on
  first boot after a flash the container never starts at all; a 55s
  timer then sets the flag and starts the container live. Superseded
  by v23/v25 (see below — this exact approach turned out incomplete).
- **Battery/suspend investigation** (separate doc, same version):
  symptom was 60% battery drain in 4 hours idle plus heavy heat and
  slow SSH. Found 303 zombie processes out of 849 total — mass HAL
  crash-loop inside the Android container (`suspend@1.0-service`,
  `bluetooth@1.0`, `gnss`, `fingerprint`, `composer-service`,
  `mediametrics`, generic `android.hardware*` binder threads), all
  children of the container's own `init`. Confirmed via manual
  `systemctl suspend` test that real suspend-to-RAM fails after
  ~360ms with `EBUSY` — the screen going dark is a pre-suspend hook
  creating the illusion of sleep, but the CPU/kernel never actually
  suspends, directly explaining both the drain and the heat.
  Unconfirmed hypothesis: the crash-looping HAL services hold
  kernel-level buses/regulators busy via runtime PM (`wake_unlock`
  sysfs showed `hal_bluetooth_lock` and `sscrpcd:415`, both matching
  the crashing-service list). Deeper diagnosis blocked by
  `CONFIG_PM_DEBUG` not being enabled in the kernel `.config` — flagged
  for a kernel rebuild, not done as of v25. **Still unresolved.**

### v23: container-gate completeness + dpkg ordering

- Found the v22 gate only covered `halium-restore-state.sh`'s own
  timer path — **9+ other independently-enabled units** each declare
  their own `Wants=`/`Requires=lxc-android-config.service`, bypassing
  the gate entirely.
- **Fix**: single point of control via `ConditionPathExists=
  /userdata/PHOSH_FIRST_BOOT_OK` directly on `lxc-android-config.
  service` itself (drop-in `00-first-boot-gate.conf`) — systemd
  re-evaluates `ConditionPathExists` on every single start attempt
  regardless of trigger source, so no per-unit patching is needed.
- **dpkg ordering bug**: `libexpat1:armhf` pre-depends on `libc6
  (>=2.38)`, but a single flat `dpkg -i *.deb` pass left
  `libc6:armhf` "unpacked but never configured" at that point,
  cascading rejections through fontconfig/cairo/pango/gdk-pixbuf/
  pulseaudio-modules-droid/pulseaudio/gnome-settings-daemon/
  gnome-control-center. Fixed with a repeated `dpkg -i
  --force-confold` + `dpkg --configure -a` sequence (run twice).

### v25: missing arm64 audio package + container auto-start abandoned

- v24's chroot install still exited 1:
  `pulseaudio-modules-droid-hidl` (arm64) requires
  `pulseaudio-modules-droid-modern` (also needs an arm64 build), but
  only the armhf variant had ever been collected into the ~290-package
  corpus — a genuinely missing file, not an ordering bug. Fixed by
  downloading the matching arm64 `.deb` from `releases.droidian.org`
  (SHA256-verified against the same version already used for armhf).
- **Container auto-start abandoned entirely**: three consecutive
  automated approaches across v22/v23/v24 (`ConditionPathExists`+
  timer, then `ExecStartPre` delay) all failed unpredictably on first
  boot after a flash (candidates: race with `/userdata`, hung
  syscalls — a hung `losetup -a` was caught live during diagnosis). By
  explicit decision, this was abandoned: the container now **never**
  starts automatically. A `CONTAINER_ENABLED` marker (not created by
  anything automatic) gates it, with a manual
  `halium-enable-container.sh` script to start it over SSH when
  wanted; `update-binary` clears the marker on every flash. (This is
  the mechanism described in the [Enabling the
  container](#enabling-the-container-after-a-fresh-flash) section
  below.)
- Reproduced the same manual "mask everything" combination that had
  worked live back in v21 (mask `halium-start-container-delayed.
  service`/`.timer` via `/dev/null` symlinks baked into
  `update-binary` itself — not as a physical symlink in
  `rootfs-files`, since `zip -9 -r` without `-y` dereferences symlinks
  at archive-build time), while leaving `halium-restore-state.sh`'s
  other functionality (wifi/volume/locale) intact.
- Kernel `.config` change: `# CONFIG_INIT_STACK_ALL_ZERO is not set`
  (see the mainline sibling project's README for the general context
  of this class of kernel-hardening-vs-vendor-driver-compat tradeoff;
  applied independently here).

### v36/v37: the real EGL root-cause fix (not a workaround)

The camera viewfinder had been black since the project began, always
attributed to "some HAL/GL plumbing issue" without a specific cause.
This version finally root-caused it at the source level.

- **Diagnostic tool built first**: a standalone C program
  (`egl_dump.c`) that opens a Wayland connection, calls
  `eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_KHR, ...)` and walks
  every `EGLConfig` the vendor driver returns via
  `eglGetConfigAttrib()`, counting how many have a non-zero, sane
  `EGL_BUFFER_SIZE`/`EGL_RENDERABLE_TYPE`. Baseline result on-device:
  **0 out of ~200 raw configs were usable** — the vendor Adreno driver
  itself was returning all-zero attribute values for every config,
  which explains a black surface with no error path (`eglSwapBuffers`
  still "succeeds").
- **Found the actual bug by reading source, not by guessing**: this
  device's deployed `libhybris` is a specific fork/branch
  (`droidian/libhybris`, `feature/next/lindroid-drm`, commit
  `60b7e82`) with a GLVND-based architecture — a single opaque
  `dpy` handle from the app's point of view is really a
  `struct _EGLDisplay { EGLDisplay dpy; EGLNativeDisplayType
  display_id; }` wrapper internal to libhybris, and every EGL entry
  point is required to unwrap it via `hybris_egl_get_real_display(dpy)`
  before passing it down to the real vendor driver.
  `hybris/egl/egl.c`'s `eglGetConfigAttrib()` was **hand-written**
  instead of going through the same code-generation macro
  (`HYBRIS_EGL_IMPLEMENT_FUNCTION4`) every sibling function uses, and
  it skipped that one unwrap call — so the vendor driver was being
  handed a raw libhybris-internal struct pointer instead of its own
  real display handle, and (reasonably) returned garbage.
- **Why this couldn't be patched with `LD_PRELOAD`**: the system
  `libEGL.so.1` is a **GLVND dispatcher**, not a normal shared library
  — confirmed via `LD_DEBUG=libs` and by `nm -D` showing the real
  vendor `libEGL_libhybris.so.0.0.0` exports *only* one symbol,
  `__egl_Main(...)`, called once at load time to register a whole
  vendor function-pointer table (`hybris/egl/glvnd/eglglvnd.cpp`).
  There is no PLT-resolved `eglGetConfigAttrib` symbol for an
  `LD_PRELOAD` shim to intercept — every call goes through that
  pre-registered table. (An earlier attempt to define
  `eglGetPlatformDisplayEXT` in an `LD_PRELOAD` library actually made
  things worse — it created a symbol binding that hadn't existed
  before, and something then called through a NULL
  `dlsym(RTLD_NEXT, ...)` result and aborted.) This ruled out every
  non-invasive workaround; a real source patch and rebuild was the
  only option.
- **The fix** — one line, in `eglGetConfigAttrib()`:
  ```c
  ret = (*_eglGetConfigAttrib)(hybris_egl_get_real_display(dpy), config, attribute, value);
  ```
- **Cross-compiling it back** required reassembling the exact build
  (fetched the fork's sources directly from GitHub at the deployed
  commit — the AOSP tree checked out under `vendor/halium/libhybris`
  locally turned out to be an unrelated, untracked, stale copy),
  a hand-written `config.h`/`android-config.h` matching the real
  `configure.ac` flags (`WANT_WAYLAND`, `WANT_LINDROID_DRM_GLOBAL`,
  `WANT_GLVND`, `GL_LIB_SUFFIX="_libhybris"`), and the GLVND dispatch
  stub generator (`glvnd/generate/gen_egl_dispatch.py`) to regenerate
  `g_egldispatchstubs.c`. Final linked `.so` matched the original's
  `NEEDED` list (one harmless extra `libdl.so.2` entry from a glibc
  version skew between the cross-sysroot and the device) and exported
  only `__egl_Main`, same as the original.
- **Confirmed fixed** with the same diagnostic tool: **105 out of 105
  good configs** after deploying the patched library (previously 0).
  This is a real, verified, source-level fix — not a config tweak or a
  workaround.

### v38: Phoc/SurfaceFlinger composer-client race

The EGL fix did not by itself make GPU rendering visible end-to-end —
`phoc` (the Wayland compositor Phosh runs on) intermittently fails to
start with a hard crash instead of a normal graceful retry:
```
write(2, "failed to create composer client", 32)
tgkill(pid, pid, SIGABRT)
```
caught via `strace -f` on a manually-launched `phosh-session` (run
with the exact environment its systemd unit sets, to reproduce the
real startup conditions). Root cause: Android's HWC (`hwcomposer`)
model only ever expects **one** exclusive composer client — normally
`SurfaceFlinger`, which Android's own `init` auto-restarts as a
persistent service inside the LXC container — so `phoc` and
`SurfaceFlinger` are structurally racing for the same single client
slot every time either one (re)starts, even with no other
interference. A partially-reliable manual recovery sequence was
worked out (`systemctl stop phosh.service` → restart
`lxc-android-config.service` → bounce the composer service → `kill -9`
every `surfaceflinger` PID inside the container via `lxc-attach` →
restart `phosh.service`), but this is a mitigation, not a fix — a
real fix would mean either making `SurfaceFlinger` not run at all
inside the container (it currently has no other job there) or
building genuine multi-client arbitration, neither attempted yet.

### v38 (cont'd): camera architecture work and the lost NV21-callback source

With the EGL fix confirmed, camera work continued but the viewfinder
stayed black — the actual bug turned out to be one layer up, in the
Qt Multimedia camera plugin (`libaalcamera.so`), and only partially
about EGL.

- **Rebuilt the camera plugin from current source, cross-compiled
  against a full Qt5 (including QtMultimedia/QtSensors, missing from
  this rootfs's own trimmed Qt install) via a custom `qt.conf`
  pointing `qmake` at a separate sysroot.** Two build-system bugs hit
  and fixed along the way: `pkg-config` silently dropping the
  cross-sysroot's own `-I` include path because it looked like "the
  default" (fixed with `PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1`, otherwise
  the compiler silently fell back to a stale, incompatible system
  header); and Qt's Meta-Object Compiler (`moc`) picking up the
  *host* g++'s default include paths instead of the cross-compiler's,
  causing `Parse error at "std"` in a totally unrelated STL header —
  fixed by invoking `moc` directly with an explicit, minimal include
  list for all 16 affected headers.
- **The rebuild compiled clean and installed, but did not fix the
  viewfinder** — comparing behavior against an old, still-working
  compiled copy of the same plugin (by hash: known-good
  `a1f73a46...` vs. today's fresh `76f1cc8a...`) showed the actual
  working rendering path never went through the GL/EGL texture route
  (`android_camera_set_preview_texture()`) at all. It uses a
  **CPU-side NV21 preview-callback path** instead
  (`AalCallbackFrameMapper`, `handleType() == NoHandle`) that copies
  each raw HAL preview frame directly into an RGB32 buffer. That
  code path exists **only inside the old compiled binary** — the
  corresponding source was lost somewhere in this project's own
  `v34→v35→v36` fork/rebase history and is not present in the
  `camera-src/` this repository ships. Recovering it (by
  disassembling the known-good binary, or by rewriting the
  callback-mode renderer from scratch against the current Qt
  Multimedia camera-control API) is the next concrete step, not yet
  done.
- Also found, independently: `QCameraImageCapture::supportedResolutions`
  (a QtMultimedia property) reads back empty even when the underlying
  C++ control's own method returns a valid fallback list, which blocks
  `Camera.start()` in the QML UI before `connectCamera()`'s own debug
  logging ever fires — a separate, deeper QtMultimedia-level gate, not
  fixed by either the EGL patch or the correct plugin.
- Migrated `halium-fix-libcameraservice.service` from a boot-time,
  phosh-startup-gated fix (60-iteration wait loop) to an **on-demand**
  one, triggered only when the camera app is actually opened
  (`halium-camera-ondemand-refresh.sh`) — removes an unnecessary fixed
  cost from every single boot for a fix that's only ever needed when
  the camera runs.
- **Screen-lock/PIN fix**: found and removed a leftover
  `require-unlock=False` override (originally added back when touch
  input during PIN entry was unreliable) that was silently
  re-disabling the lock screen's password requirement on every fresh
  flash, *after* the real underlying touch/`/etc/shadow` group bug it
  was working around had already been separately fixed — the two
  fixes had been quietly fighting each other for months. Confirmed
  live: lock screen requires and accepts a PIN normally on a clean
  flash with the override removed.

### Known unresolved: telephony (RIL/modem)

Dial pad stays greyed out — `org.ofono.Manager.GetModems` returns zero
modems, unchanged since v11. Two real, confirmed bugs already found
and fixed live on the device (only partially folded back into
sources — see `v38-latest/DROIDIAN-V25-PLAN.md` for the precise,
version-accurate state):

1. `ofonod-wrapper`'s plugin auto-selection shells out to a
   `device-info` binary that doesn't exist on this system, silently
   falls through to the wrong (`ril`, non-functional here) plugin as a
   result. Same root-cause *category* as the "oFono binder plugin"
   fix already solved on this device's separate Ubuntu Touch/Halium
   project — different trigger, same underlying binder-vs-ril
   plugin-selection failure mode. Hardcoded the plugin selection to
   `binder` as a workaround.
2. After fixing (1): `ofonod` immediately failed on `Invalid user
   'radio'` — this Debian rootfs has no `radio` system user (Android
   hardcodes `AID_RADIO`/UID 1001); `useradd`/`adduser` aren't present
   in the image either, so the account was added by hand directly in
   `/etc/passwd`/`/etc/shadow`/`/etc/group`.

Even with both fixes applied, modem count is still zero with no error
surfaced in the log. Current leading theory (unconfirmed): `ofonod`
runs as plain `root` rather than switching to the newly-created
`radio` UID before it touches `/dev/hwbinder`, and the real Android
RIL process's hwbinder permissions are keyed off that specific UID.
Next diagnostic step noted in the source docs: turn on `ofonod`'s
verbose/debug logging (`-d` flag or `OFONO_DEBUG`) since the log
currently goes silent immediately after "Excluding RIL modem driver"
with no indication of what happens on the actual binder connection
attempt.

## Recurring themes

Patterns worth knowing before debugging the next regression — several
of these bit the project more than once, in different disguises:

1. **`PATH=` leaking from the container environment into host-side
   hook scripts** (`mount.sh` in v2/v5/v7, TWRP's `update-binary`
   chroot shell in v22) silently broke shell built-ins for large
   unknown spans of project history. Two distinct flavors of the same
   trap: guest `lxc.environment` leaking into a host script, and
   TWRP's minimal `PATH` lacking `/usr/bin` for chrooted Debian tools.
2. **The `sync_dirs()`/writable-paths "trust problem"**: any live edit
   to a file under a synced writable-path (`/etc/systemd/system`,
   etc.) is invisible on the next boot unless the *persistent*
   `/userdata/system-data/...` copy is also edited. Bit the project
   repeatedly (v2, v5/v6, v9, v11), eventually generalized in v11 into
   a documented "check all 3-4 injection locations" checklist rule —
   the same lesson as the M52 mainline-kernel sibling project's own
   "sed -i breaks bind-mount" finding, different mechanism, same
   underlying shape of bug.
3. **Container auto-start reliability was never actually solved** —
   attempted via wait-scripts (v8), delay+sleep tuning (v9/v11),
   `ConditionPathExists`+timer gating (v22/v23), `ExecStartPre` delay
   (v24), and finally abandoned for manual-only start (v25). Each fix
   "worked" in isolated live tests but failed unpredictably on the
   next clean flash.
4. **Zombie/crash-looping Android HAL services** (`suspend@1.0`,
   `bluetooth@1.0`, `gnss`, `fingerprint`, `composer-service`,
   `mediametrics`, `media.c2`, `vdc`) recur from v6 through v22 under
   different framings (overheating, `hwservicemanager` spam, battery
   drain, failed suspend) — likely one systemic root cause (Android
   HAL layer instability inside the container) manifesting as several
   distinct symptoms, never fully root-caused as a single fix.
5. **linkerconfig/APEX namespace visibility bugs recurred at multiple
   config layers**: the top-level `ld.config.txt` fix (v2/v4) and then
   the *separate*, nested `com.android.art/ld.config.txt` fix
   (v11/v12) were the same category of bug at two different levels of
   the same mechanism.
6. **Binary/bytecode live patches silently losing effect due to
   caching**: `.odex`/`.vdex` AOT-cache invalidation and `zip -j`
   silently failing to update `classes.dex` are both "the patch looks
   applied but isn't actually running" traps — check both whenever a
   smali/bytecode patch appears not to take effect.
7. **Audio was fixed and re-broken multiple times, at different
   layers, across the whole project**: kernel-level fix (v10,
   `CONFIG_QCOM_APR`) → PipeWire couldn't route the card at all (no
   UCM2 profile, v11) → discovered a prior working PulseAudio-based
   fix had silently regressed when the stack switched to PipeWire
   (v11) → full PulseAudio+armhf bridge rebuilt from scratch (v21) →
   broken by overlayfs+dpkg interaction on the first live flash (v21)
   → dpkg dependency-ordering bug (v23) → missing arm64 package (v25).
   Probably the single most-recurring subsystem in the whole project
   — expect it to break again at the next major stack change.
8. **TWRP/toybox shell quirks** caused multiple silent failures:
   heredocs producing empty files (v4/v5), the 4GiB `unzip` boundary
   (v4/v5), and the minimal `PATH` in the `update-binary` shell (v22)
   — all are "the flashing environment's shell is not a normal shell"
   traps; assume nothing about it that isn't tested.

## Useful commands

```sh
# ground-truth container state (systemd's own view can lag/disagree during restarts)
lxc-info -n android
sudo systemctl status lxc-android-config.service
```

### Enabling the container after a fresh flash

```sh
touch /userdata/CONTAINER_ENABLED
sudo /usr/libexec/halium-extras/libexec/halium-enable-container.sh   # or:
sudo systemctl start lxc-android-config.service
```
As of v25, the container is intentionally **never** started
automatically (see the v25 history entry above for why three earlier
automated approaches were abandoned) — `update-binary` clears this
marker on every flash, so it's a one-time step per flash, not per
boot.

### Finding what's actually consuming CPU/heat

```sh
ps aux --sort=-%cpu | head -20
cat /sys/class/thermal/thermal_zone*/temp   # millidegrees C
```
Given the confirmed-broken suspend and the recurring zombie-HAL-
service pattern (see [Recurring themes](#recurring-themes) items 3–4),
worth running whenever the device feels hot with no obvious cause.

## Flashing

The installer is a standard TWRP-flashable ZIP built from the contents
of a versioned folder (this repository mirrors the `v38` staging
state, the latest at time of writing). First obtain the proprietary
vendor blobs listed in `PROPRIETARY-FILES.txt` from your own device
and place them at the same relative paths, then drop `system.img`,
`rootfs-chunks/` and `userdata-overlay.tar.gz` into a `local-large-
files/` folder next to `v38-latest/` (see [Repository
layout](#repository-layout) above — the tracked symlinks under
`v38-latest/firmware/` and `v38-latest/data/` pick these up
automatically), then:

```sh
cd v38-latest && zip -9 -r ../output.zip . -x ".*"
```

(must be run from *inside* the version folder — running it from
outside with the folder name as an argument embeds a path prefix that
breaks TWRP's `update-binary` lookup; also must **not** use `-y`
per the v25 history entry above, since the symlinks above need to be
dereferenced into real content at archive-build time, not preserved as
symlinks pointing outside the archive).

The `rootfs-chunks/` files are expected to be named
`rootfs.img.part-0`, `rootfs.img.part-1`, ... (produced by `split -b
<size> rootfs.img rootfs.img.part-` — see the v3/v4 history entry
above for why chunking is needed at all: TWRP's own `unzip` has a hard
4GiB boundary bug). None of this vendor-derived binary material is
included in the repository itself — this repository is the
patch/fix source material and full project history, not a turnkey
installer.

## License

No specific license asserted for the original scripts/configs in this
repository (personal project). Individual injected files retain
whatever license their upstream project uses (systemd unit
conventions, standard Debian package file formats, Droidian's own
package builds, etc). Not for redistribution of the excluded
vendor-derived binary material listed in `PROPRIETARY-FILES.txt`
(`system.img`, kernel modules tarball, rootfs/userdata image chunks,
Samsung/Qualcomm/ArcSoft `.so`/firmware blobs).
