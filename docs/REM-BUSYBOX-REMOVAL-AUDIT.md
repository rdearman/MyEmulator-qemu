# REM BusyBox Removal Audit

Audit date: 2026-10-09. This is a read-only inspection of the desktop reference
image `REM-FLIGHT-HUMAN-UAT-FULL/rootfs.ext4`; no existing image was changed.

## Reference image

| Item | Value |
|---|---|
| Image | `REM-FLIGHT-HUMAN-UAT-FULL/rootfs.ext4` |
| Image SHA-256 | `9e3f2c25f28205fab79722b9c546399f28cb30e065fc583d1612d6521126f20e` |
| Read-only `e2fsck -fn` | PASS, exit 0 |
| `/bin/busybox` SHA-256 | `9e7acc1db28a5506215293e3d0af19ab734bcb527d5145b45372c2963ba9e31d` |
| `/usr/bin/busybox-qualified-20260928` SHA-256 | `1d8ae8c351cedffdea2473c3d8108725be39c4f6899751d92af931044ad4d1b5` |
| BusyBox version | 1.37.0 (desktop image builds) |

`/sbin/init`, `/sbin/getty`, and `/sbin/login` are symlinks to
`../bin/busybox`. `/bin/sh`, `/bin/ash`, `/bin/mount`, `/bin/umount`, and the
utility aliases below resolve to `busybox`. `/etc/inittab` is specifically a
BusyBox init configuration. The `/etc/init.d/rcS` → `rc` SysV dispatcher is a
small shell framework, not a second init implementation.

The default boot path is `/etc/inittab` → `/etc/init.d/rcS` →
`/etc/init.d/rc start 3`. The REM 0.2 candidate replaces the inittab consumer
with its own PID 1, while preserving the SysV `rcS`/`rc` dispatcher. Runlevel links include `S40network` and `S99local`.
The network script currently declines to configure an interface when
`ifconfig` or `udhcpc` is unavailable; network startup is not qualified by this
image. `S99local` invokes `/etc/rc.local` when present. `/etc/inittab` also
delegates console respawn, Ctrl-Alt-Del, shutdown, and restart to BusyBox init.

## Dependency and replacement register

| Command/service | Current invocation or location | Planned replacement | Replacement source/package | Status |
|---|---|---|---|---|
| musl runtime and SDK | `/usr/lib/libc.a`, startup objects, and `/usr/include` in the pinned seed | Record as shared `rem-musl` seed package; replace payload with native package later | `distribution/packages/rem-musl` | 1,592 pinned seed files verified by size and SHA-256 during assembly |
| PID 1 init | `/sbin/init -> ../bin/busybox`; `/etc/inittab` | Small REM PID 1; retain SysV runlevel model | `distribution/packages/rem-system` | Candidate boots as PID 1; isolated poweroff passes; reboot/reaping qualification pending |
| SysV boot dispatcher | `/etc/init.d/rcS`, `/etc/init.d/rc` use `/bin/sh`, `mkdir`, `rm`, test/glob operations | Preserve and review scripts; `/bin/sh` becomes Bash; utilities become standalone | Existing scripts plus GNU coreutils package | Candidate starts the dispatcher; coreutils contains 48 manifest entries including `rm`; utility runtime qualification pending |
| Console getty | `ttyMY0::respawn:/sbin/getty 115200 ttyMY0 linux` | REM serial getty that sets controlling tty and execs login | `distribution/packages/rem-system` | Candidate prints login prompt; authenticated session pending |
| Login/account lookup | `/sbin/login -> ../bin/busybox`; `/etc/passwd`, `/etc/shadow` | Small shadow-aware REM login; keep account creation tools out of boot path | `distribution/packages/rem-system`; musl `libcrypt` | Built; desktop SHA-512 password check faults in `crypt_sha512`, so login is not qualified |
| Shell | `/bin/sh`, `/bin/ash -> busybox`; `/bin/bash -> ../usr/bin/bash`; `/usr/bin/yash` | Bash for scripts, Yash as user shell; remove ash alias | Existing REM Bash/Yash package payloads | Present in reference; candidate runtime tests pending |
| Mount/umount | `/bin/mount`, `/bin/umount -> busybox`; currently used indirectly by boot setup | REM mount helper for proc, sysfs, devtmpfs, devpts; direct init mounts are preferred | `distribution/packages/rem-system` | PID 1 directly mounts devtmpfs, proc, sysfs, devpts, `/run`, and `/tmp`; mount sentinels pass in QEMU |
| Hostname | `/etc/hostname`; no standalone hostname executable in the audited base | `rem-hostname` using `sethostname(2)` | `distribution/packages/rem-system` | Helper reports `ENOSYS` from the current kernel; hostname not qualified |
| Shutdown/reboot/halt | `/bin/sync` and `/sbin/{reboot,halt}` BusyBox links; `/etc/inittab` action hooks | REM shutdown helper: run SysV stop links, sync, reboot syscall | `distribution/packages/rem-system` | Isolated poweroff executes stop links and passes `e2fsck -fn`; reboot request does not produce a second boot |
| Network startup | `/etc/init.d/S40network`; expects `ifconfig lo` and optional `udhcpc`; not linked into the current default runlevel | REM static QEMU user-network helper; keep an explicit SysV hook | `distribution/packages/rem-network` | Hook runs, but current QEMU machine provides no guest NIC (`eth0` missing); networking qualification blocked |
| Syslog/klog | No `syslogd`/`klogd` invocation or default runlevel service found | Keep kernel log on the REM console initially; add a standalone logger only if required | REM system package (future) | No boot dependency |
| Cron | No `crond` invocation or default runlevel service found | Standalone cron package only if a distribution service needs it | Native userland package (future) | No boot dependency |
| File/core utilities | `/bin` BusyBox aliases: `cat chgrp chmod chown clear cp cut date df du echo env false head id ln ls mkdir mv od printf pwd rm rmdir sleep sort stat stty sync tail tee test touch tr true uniq wc whoami`; `top` is also an alias | GNU coreutils binaries, plus a small terminal helper where needed | Existing native build output from `.rem-flight-build-20260924/rootfs/usr/bin`; versions pinned by native-userland plans | 48 binaries imported with size/SHA records; core command runtime qualification is blocked by REM faults in other packages |
| Text/search utilities | `/bin/{grep,sed}` and `/usr/bin/{grep,sed}` are BusyBox aliases | Standalone GNU grep and sed | Existing native build output; GNU grep 3.11, sed 4.9 plans | Installed; `grep -qx` and `sed -n '2p'` fault under the current kernel |
| File traversal | `/bin/find`, `/bin/xargs` BusyBox aliases | Standalone GNU findutils | Existing native build output; GNU findutils 4.10.0 plan | Installed; `find /etc -name hostname` faults under the current kernel |
| AWK | `/usr/bin/awk -> ../../bin/busybox` | Standalone GNU awk | Existing native build output; gawk 5.3.1 plan | Payload import and candidate tests pending |
| Archive/compression | `/bin/{tar,gzip}` BusyBox aliases; gzip alias also under `/usr/bin` | GNU tar, gzip, bzip2, xz payloads | Existing native build output; pinned plans in `toolchain/native-userland/kit/plans` | Payload import and candidate tests pending |
| Editor | `/bin/vi -> busybox`; `/usr/bin/vim` and `vim.real` are standalone | Keep the qualified REM Vim package; remove only the BusyBox `vi` alias or map it to Vim | `tools/rem-flight-image/staging/applications/vim-rem-9.1.1724` | Installed; runtime smoke did not reach editor tests |
| Toolchain | No BusyBox dependency for `/usr/bin/{as,ld,ar,ranlib,readelf,nm,objdump,strip,chibicc,samu,make}` | Keep native REM packages and manifests | Binutils and chibicc/Samurai staged package artifacts; native-userland build plans | Present in candidate; runtime smoke did not reach these tests |
| curl/TLS | Standalone `/usr/bin/curl`; TLS assets under `/etc/ssl` | Retain the selected REM curl/BearSSL package after runtime check | Existing desktop artifact and native-userland curl/BearSSL plans | Present; runtime/network check pending |
| SSH | No Dropbear executable is in the audited `/usr/bin` or `/usr/sbin`; an OpenSSH `sshd` exists in an older native build tree | Prefer Dropbear; do not substitute BusyBox applets | Dropbear 2024.86 pinned native-userland plan | No standalone payload/source archive in seed/workspace; package is marked pending |
| Python | `/usr/bin/python3` is a symlink to `python3.12`, not BusyBox | Keep standalone Python runtime if package selection requires it | Existing reference image | Not required for boot; not audited for this goal |

### BusyBox-linked command inventory

The audited `/bin` aliases are: `addgroup`, `adduser`, `ash`, `cat`, `chgrp`,
`chmod`, `chown`, `clear`, `cp`, `cut`, `date`, `df`, `du`, `echo`, `env`,
`false`, `find`, `grep`, `groups`, `gzip`, `head`, `id`, `ln`, `login`, `ls`,
`mkdir`, `mount`, `mv`, `od`, `passwd`, `printf`, `ps`, `pwd`, `rm`, `rmdir`,
`sed`, `sh`, `sha256sum`, `sleep`, `sort`, `stat`, `stty`, `su`, `sync`,
`tail`, `tar`, `tee`, `test`, `top`, `touch`, `tr`, `true`, `umount`, `uname`,
`uniq`, `vi`, `wc`, `whoami`, and `xargs`. `/sbin` additionally links
`addgroup`, `adduser`, `delgroup`, `deluser`, `getty`, `halt`, `init`, `kill`,
`login`, `reboot`, `setsid`, `sulogin`, and `umount` to BusyBox. `/usr/bin`
aliases include `awk`, `gzip`, `sed`, `sha256sum`, and `vi`; Python's `python3`
alias resolves to `python3.12` instead.

## Candidate result (2026-10-09)

The staged v3 REM Linux 0.2 candidate passes the post-ext4 physical-file and
symlink audit: neither known BusyBox binary hash nor a BusyBox-named file
remains, and no init script references BusyBox. It was booted under desktop
QEMU; REM PID 1 started, `/proc`, `/sys`, `/dev/pts`, and devtmpfs checks passed,
the SysV startup hook ran, and getty printed the login prompt. Qualification
fails at login because SHA-512 password verification faults in musl `crypt`.
Independent in-guest tests then showed faults in standalone grep, sed, and
find; repeated faults trigger a kernel panic. The machine has no guest NIC,
hostname setting returns `ENOSYS`, and gawk/ncurses/Dropbear payloads are
pending. Isolated poweroff passes, but reboot does not complete a second boot.

## Qualification gate

The booted candidate is physically BusyBox-free, but it is not a qualified
REM Linux distribution. Release qualification still requires authenticated
login, stable native utility execution, working guest networking, the pending
packages, clean reboot, and a complete userland test suite. The separate
poweroff test passes with a clean filesystem check after shutdown.
