# REM system administration

## Scope and status

This guide describes the current REM Linux flight rootfs and its limits. It
is not a distribution administration manual: several familiar account,
service, and storage tools are not configured. Check
[`REM_SOFTWARE_INVENTORY.md`](REM_SOFTWARE_INVENTORY.md) and
[`REM_USER_MANAGEMENT.md`](REM_USER_MANAGEMENT.md) before relying on a command.

## Filesystem layout

The inspected flight rootfs uses conventional directories such as `/bin`,
`/sbin`, `/etc`, `/dev`, `/proc`, `/sys`, `/root`, `/usr/bin`, and `/usr/lib`.
Native toolchain integrations have also used `/root/native/bin` and
`/root/native/lib`; those paths are not guaranteed in every image. The ext4
rootfs is persistent across normal guest reboots. Treat each published image
as an immutable seed: shut down REM before making backups or modifying its
host-side image file.

## Boot and shell ownership

The audited development init is a project shell script, not a service
manager. It sources `/root/.profile`, starts a periodic sync loop, changes
directory, then attempts an interactive shell and falls back to BusyBox ash.
The audited image starts this path as root. The exact script and shell vary by
candidate; inspect `/sbin/init` and the image's integration manifest before
troubleshooting. The distribution seed now provides BusyBox init
configuration with `/etc/inittab`, `/etc/init.d`, `/etc/default`, and SysV
runlevel directories. It has not been integrated into or booted from the
audited image. See [`REM_INIT_SYSTEM.md`](REM_INIT_SYSTEM.md).

The current image's recovery path is its root console shell. The intended
distribution service model is script-driven SysV style under `/etc/init.d`;
there is no systemd or alternative service manager. Getty/login, normal-user
access, and runtime runlevel changes still require same-image qualification.
The shell may lack job control if init has not established a session and
controlling terminal.

## Environment

Common tested values for Yash are:

```sh
PATH=/bin:/usr/bin:/sbin:/usr/sbin
HOME=/root
TERM=linux       # console default; use xterm only when the terminal really is xterm
```

Toolchain candidates may prepend `/root/native/bin`. A default user profile
is provided in `toolchain/userspace/rem-skel/`; it is a template for image
integration, not evidence that every current image installs it. See
[`REM_YASH_PROFILE.md`](REM_YASH_PROFILE.md).

## Processes and services

Use `ps` to inspect processes when the configured BusyBox applet supports the
desired format. Use `kill PID` to signal a process. The boot shell's periodic
sync helper is image-specific. There is no service manager in the inspected
base image. The SysV dispatcher and runlevel policy are staged separately and
enable no daemons by default. See [`REM_CONFIGURATION.md`](REM_CONFIGURATION.md)
for `/etc/default`.

## Hostname, time, storage, and logs

Hostname setup is image-specific; inspect `/etc/hostname` and the init script
before relying on `hostname`. The validated Make guest emitted a timestamp
far in the future, so wall-clock correctness is not established. Do not use
guest timestamps as trusted provenance until the timer/clock setup is
qualified.

The root filesystem is ext4 attached through the QEMU machine's storage
configuration. Current block-device design notes are explicitly proposals;
the QEMU command line and kernel config for a specific tuple are authoritative
for which device is actually attached. Use `sync` before a clean shutdown.
Do not copy, resize, or run filesystem repair against an image while QEMU has
it open. See [`REM_EXT4_PERSISTENCE.md`](REM_EXT4_PERSISTENCE.md).

No general journald/syslog service is established. Kernel and init messages
appear on the console; capture the QEMU console transcript when diagnosing a
boot. Confirm whether `/var/log` exists and is writable before expecting log
files.

## Troubleshooting and recovery

1. Preserve the original image; test on a copy or qcow2 overlay.
2. Record the QEMU command, kernel/rootfs hashes, and console output.
3. If a shell profile breaks startup, move or rename that file from a stopped
   guest or mount a disposable copy offline; retain the emergency shell path.
4. If ext4 warnings occur, stop writes, shut the guest down, then run a
   read-only check (`e2fsck -fn`) on a detached copy.
5. If a compiler or shell fails, verify the executable's hash and matched
   runtime tuple before changing the kernel or replacing binaries.
6. For host-side development, keep an offline backup and verify file hashes
   after restore. See [`REM_OFFLINE_WORKFLOW.md`](REM_OFFLINE_WORKFLOW.md).

## Administrator features still needed

Normal-user login, account/password management, privilege elevation,
predictable clock setup, a documented service startup mechanism, and a
qualified block-device administration path remain integration work.
