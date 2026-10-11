# REM application compatibility audit

This audit uses the disposable REM tuple and does not modify
`REM-FLIGHT-TTY`.

## Current evidence

| Workload | Result | Evidence |
|---|---|---|
| Yash | PASS for noninteractive shell execution | `/usr/bin/yash -c 'echo YASH_OK; pwd; export T=ok; echo $T'` returned status 0 and produced the expected output. Interactive prompt/editing remains covered by the separate Yash acceptance record. |
| Kilo | PASS | Canonical Kilo edit/save/reopen/reboot acceptance is recorded in `REM_YASH_STATUS.md` and `/tmp/rem-kilo-upstream/`. |
| GNU ld | PASS | `ld --version` returned 0 in the focused run. |
| objdump | PASS | `objdump --version` returned 0. |
| readelf | PASS | `readelf --version` returned 0. |
| GNU make | PASS for `make --version` | Version startup returned 0; the known `make -r -f /dev/null` crash remains a separate existing limitation. |
| BusyBox ls | PASS | `/bin/busybox ls /` returned 0. |
| native `as` (old rootfs copy) | PENDING | The old rootfs copy crashed at PC `0x021412d0`; it predates the canonical corrected toolchain. |
| native `as` (canonical staged copy) | PASS | Replaced only in the disposable rootfs with `toolchain/flight-kit-runtime/bin/as` (SHA-256 `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93`); `as --version` returned 0. |
| chibicc-new | PENDING | `/root/chibicc-new -c /root/hello.c` returned 129; source/build status is owned by the chibicc workstream. |
| samu | PENDING | `/root/samu --version` returned 135 with `MYEMU_USER_ALIGN`; current ABI/runtime status is owned by the Samurai workstream. |

The broad legacy compatibility script also exposed crashes in several old
Coreutils, Findutils, Bash, Python and Emacs binaries. Those binaries are
provenance-mixed and are not current debugging targets until rebuilt with the
canonical corrected compiler.

## Concrete syscall compatibility change

The focused run showed old binutils invoking generic syscall 153 (`times`) and
165 (`getrusage`), which were absent from the MyEmulator2 dispatcher. The
dispatcher now maps both to Linux's `sys_times()` and `sys_getrusage()` in the
development kernel source. Rebuilding the old `as` binary still crashed in its
own internal path, so the syscall addition is retained as compatibility
coverage but is not claimed to fix that stale executable.

The same trace showed Yash startup probing syscall 48 (`faccessat`) and 439
(`faccessat2`). Both are now mapped to the corresponding Linux implementations.
A focused Yash probe still returns `YASH_STATUS:0`, with no remaining unknown
48/439 entries.

## Flight workload matrix

| Workload | Status | Current boundary |
|---|---|---|
| Yash | PASS | Noninteractive command execution passes; interactive acceptance is recorded separately. |
| Kilo | PASS | Canonical edit/save/reopen/reboot acceptance already passed. |
| chibicc | PENDING | Owned by the chibicc workstream; current staged binary still fails. |
| Binutils (`as`, `ld`, `objdump`, `readelf`) | PASS with canonical staged `as` | `ld`, `objdump`, and `readelf` passed; the canonical staged assembler passed. Do not use the old rootfs `as`. |
| Samurai dependencies | PENDING | `samu` remains blocked by its current ABI/runtime issue; no kernel syscall gap was demonstrated. |
| BusyBox tools | PASS | BusyBox `ls` and the network-capable utility set used by the disposable image passed the exercised paths. |
| Small native C programs | PASS for existing smoke artifacts | Existing native hello/malloc and flight-kit smoke evidence remains valid; no new syscall gap was found in this pass. |

No networking, virtio, Yash source, Kilo source, binutils implementation, or
flight image was changed.

## Hostile pre-flight break test

The disposable pre-flight launcher was run against the current development
tuple, with output captured at `/tmp/rem-preflight.log`:

| Exercise | Result | Evidence and classification |
|---|---|---|
| Yash command execution | PASS | `YASH_STATUS:0`; `echo`, `cd`, export and variable expansion completed. |
| BusyBox filesystem operations | PARTIAL / BREAK | `mkdir`, `cat`, `ls`, `test`, and `sync` completed, but the disposable rootfs `cp` and `mv` applets each terminated with `Segmentation fault` and `MYEMU_BAD_USER_FAULT` at low address `0x30c`. This is a concrete userspace/runtime compatibility issue requiring provenance isolation; it is not evidence to change Yash or networking. |
| Native assembler workflow | NOT VALIDATED | The probe changed to `/root/assembly-project`, but that image did not contain `hello.s`; `as` therefore reported the legitimate missing-input error (`AS_STATUS:129`). The subsequent `ld` invocation had no object and faulted, so this is an invalid workflow probe rather than an assembler or kernel conclusion. |
| GNU make | FUNCTIONAL LIMITATION | `make -r -f /dev/null` emitted the known timestamp warning and `No targets. Stop.` (`MAKE_STATUS:130`). No makefile was supplied, so this does not establish a new build failure. |
| Terminal interaction | PASS by existing acceptance | The pre-flight launcher was noninteractive. Canonical Kilo edit/save/reopen/reboot and Yash interactive acceptance remain the authoritative terminal evidence. |

The `cp`/`mv` faults are the only new hostile-test break. Their binaries and
the BusyBox build provenance must be checked before attributing the low-address
fault to the kernel; no new syscall gap was observed. The test also emitted
the already-known EXT4 `page ... does not have buffers attached` warnings,
which remain a separate persistence-workstream issue.

### Pre-flight artifact tuple

- Kernel: `.rem-phone-deploy-20260925/linux-network-build/vmlinux`, SHA-256
  `1aa9d65e2ba5910a197ebed06e0189b87723d23c04b9594bcfdf0bc01bad278b`
- QEMU: `.qemu-build/qemu-system-myemulator32`, SHA-256
  `60d869564f26ed2d63f20830a291b67443cf323faa3d3f4b81a94f52534e6882`
- Disposable rootfs: `/tmp/rem-dev-flight-stage/rootfs.ext4`, SHA-256
  `81d281c3ee36626f5ae80b11453e0d1720f767266c827c282a2c88a6f82c43bd`
- Persistent log: `/tmp/rem-preflight.log`

## BusyBox `cp`/`mv` follow-up

The failing pre-flight image used `/bin/busybox` extracted from the disposable
rootfs. Its exact SHA-256 is
`d9e89176978371aa890af591b45e26823e1d0e39005f08ade175fd23bb05fa3d`.
It is a statically linked BusyBox v1.37.0 image dated 2026-09-25; no embedded
compiler revision is present, so its corrected-GCC provenance cannot be proven.

Two disposable replacement checks were made without changing the flight image:

| Replacement | Result |
|---|---|
| `/tmp/rem-network-test/busybox-script-build/busybox` (`6be19cb921fd1f7e47a7d9f76c37fe0ac2e286f86f03c14db9a583c6d452c447`) | The larger ELF faults in the kernel ELF execution path before the probe starts (`pc=0x007b3d8c`, kernel page fault). This is an ELF-loader/runtime boundary, not evidence that its `cp` applet is correct. |
| `/tmp/rem-network-test/busybox-build/busybox` (`ff6ace643064a819eb5dfec9e540e21da200c195b29240deba19ffafd6c621de`) | The probe reaches `CP_BEGIN` but does not complete `cp` before the disposable timeout; no `MYEMU_BAD_USER_FAULT` was observed in that run. |

Therefore the stale-artifact hypothesis is **not yet proven**. The original
`cp`/`mv` crashes remain a reproducible userspace compatibility boundary, while
the candidate replacements expose separate ELF-loader or blocking behaviour.
No kernel, libc, BusyBox source, or flight artifact was modified. Further work
requires a clean canonical BusyBox/rootfs pair and a narrower `cp` probe before
attributing responsibility to the kernel, compiler, or libc.

The subsequent minimal probe was not counted as evidence: the repeatedly
debugfs-modified disposable image stopped during early boot before `/sbin/init`.

## Final flight-candidate provenance audit

The selected production candidate was audited read-only; no flight artifact was
changed.

| Component | Artifact and SHA-256 | Assessment |
|---|---|---|
| BusyBox | `REM-FLIGHT-TTY/rootfs/rem-rootfs.ext4:/bin/busybox` — `d9e89176978371aa890af591b45e26823e1d0e39005f08ade175fd23bb05fa3d` | This is the same BusyBox image involved in the unresolved disposable `cp`/`mv` faults. It has no validated `cp`/`mv` acceptance result. |
| libc | `/usr/lib/libc.a` extracted from the flight rootfs — `30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924` | The rootfs has no `/lib` directory or shared libc; the workload is statically linked against the archived musl libc. Exact compiler provenance is not encoded in the archive. |
| Kernel | `REM-FLIGHT-TTY/kernel/vmlinux` — `e2516e8af9ffc0ee6ddb2113592f98d6c65f9b376961ee99c2ab24bc5fa546de` | Production flight kernel tuple. |
| QEMU | `REM-FLIGHT-TTY/qemu/desktop/qemu-system-myemulator32` — `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` | Production desktop QEMU tuple. |
| Rootfs | `REM-FLIGHT-TTY/rootfs/rem-rootfs.ext4` — `eff360514d9bb49d214cf2b60dd656524fdff493e7354249e5edf8e7d43f12ce` | Current production rootfs. |

**Packaging conclusion:** the selected flight candidate contains the known
failing/unvalidated BusyBox, not a validated replacement. No BusyBox `cp`/`mv`
acceptance result is currently available for the production image. This is a
provenance finding only; no kernel or userspace source fix is justified by the
available evidence.

## REM 0.1 login syscall audit — setgroups

| Syscall | Number | Status | First affected workload | Fix and validation |
|---|---:|---|---|---|
| `setgroups` | 159 | Implemented in the disposable REM 0.1 kernel | BusyBox `login` after password authentication | `linux/arch/myemulator2/kernel/syscall.c` now dispatches to Linux's generic `sys_setgroups(int, gid_t __user *)`, retaining standard count, user-pointer, and permission checks. The kernel built successfully; symbol table contains `sys_setgroups`. Full login-to-Yash acceptance remains pending because the RC3 password path did not complete in the available run. |

The RC3 run reached the `dev` login and `Password:` prompt without
`ENOSYS`, but did not reach the post-authentication `setgroups` call before
the disposable test was stopped. This is therefore an implementation/build
pass, not a claim that the complete login workflow is accepted. No rootfs or
flight image was modified.

## Login authentication boundary

The disposable RC3 account files are internally consistent:

- `/etc/passwd`: `dev` uses UID/GID `1000`, home `/home/dev`, shell
  `/usr/bin/yash`;
- `/etc/shadow`: `dev` has a SHA-512 crypt record with the `remdev` salt;
- `/etc/group`: `dev` is a member of `users`.

The BusyBox build enables shadow passwords, internal shadow lookup, internal
crypt, and SHA-256/512 crypt. Traces show `login` accepts the username,
prints `Password:`, and then blocks in its tty read; no authentication result,
`setgroups(159)`, or `execve(/usr/bin/yash)` follows. This run therefore
isolates the current boundary to password-input delivery or authentication
runtime, rather than another missing syscall. The account/hash configuration
is not evidence of a kernel defect by itself.

The synchronized PTY run delivered every password byte successfully: the
kernel trace contains one-byte reads for `r`, `e`, `m`, `t`, `e`, `s`, `t`,
and the terminating newline. Login then reads the 138-byte shadow record and
performs no further syscall before timeout; there is no `setgroups(159)`. The
host verifies that `remtest` matches the recorded SHA-512 crypt hash. The
remaining failure is therefore inside BusyBox's internal SHA-512 password
verification path (or its generated runtime code), not tty delivery and not
a missing syscall. A separately rebuilt and qualified login binary is needed
before attributing this to the kernel.

A disposable empty-password shadow test reached `setgroups(159)` and produced
the `REM>` Yash prompt with the same kernel, tty, account, and login binary.
Replacing the hash with SHA-512, SHA-256, or MD5 caused login to stop before
syscall 159. This isolates the remaining defect to non-empty BusyBox internal
`crypt()` execution (or its generated code); it is not caused by account
lookup, tty input, `setgroups`, or Yash startup. These hash-modified images
were disposable diagnostics only and were not used to qualify the flight
image.

A disposable BusyBox rebuild with `-O0` produced
`/tmp/rem-busybox-o0/busybox` (SHA-256
`262a644699aac3d64a94c6ccded9f384b47cee593c361fcbd81f2d88eae91033`), but
the original SHA-512 login still stopped before `setgroups`. This rules out a
simple optimization-level workaround. QEMU monitor sampling during the stall
reached the kernel interrupt handler, but exposed no userspace fault or new
missing syscall.

An additional disposable reduction changed BusyBox `ROUNDS_DEFAULT` and
`ROUNDS_MIN` to `1` in a copied source tree and rebuilt the crypt-enabled
binary (`/tmp/rem-busybox-cryptone/busybox`, SHA-256
`1d8ae8c351cedffdea2473c3d8108725be39c4f6899751d92af931044ad4d1b5`). Login
still failed to reach syscall 159. The stall is therefore not caused by the
5000-round password-cost setting; it occurs in the underlying internal hash
path or generated arithmetic/runtime code.

To reduce the failure further, a disposable BusyBox build with the `cryptpw`
applet enabled was produced (`/tmp/rem-busybox-cryptprobe/busybox`, SHA-256
`438296bd1d9143e8b6fe8d38c0b441b2d7b8063ce03c8b59abf94c1c289c49a1`). The
login control with an empty password still reaches `setgroups`; attempts to
invoke the interactive `cryptpw` command were limited by the separate Yash
input-delivery path. No kernel syscall error was exposed by the reduction.

## Standalone SHA-crypt probe (corrected boundary)

The earlier one-round login reduction used a one-round BusyBox binary with the
original 5000-round shadow hash, so its failure was not evidence that one-round
crypt itself failed. A standalone probe was therefore built from the canonical
BusyBox 1.37.0 sources (`libbb/hash_md5_sha.c`, `libbb/pw_encrypt_sha.c`,
`libbb/bb_bswap_64.c`) with only the required support functions.

Host and REM both produced the same one-round result for `remtest` and
`$6$remdev$`:

```
$6$remdev$Egrm6z1lIgJsvpUNmT2VwLky59It4uvzggB5Qd9.Z8johEzWcNxTEbeFJBtLhmUBM/e.JZcW1eRGPFRQ0sNlz0
```

The REM executable was `/tmp/rem-sha-one` (ELF32 MyEmulator2); the host probe
was `/tmp/rem-sha-one-host`. A 100-round probe also matched byte-for-byte:

```
$6$remdev$/p1P2oJBH5xD/9SiU2pWOBeTCooEN6HgQJ8RehjD1sZgA37Bt9xVZB54XbvZ7guA5OmlvLDp63SYxYD4BbmiY0
```

A disposable login image using the one-round BusyBox binary and the matching
one-round shadow hash reached `setgroups(159)` and the `REM>` Yash prompt.
Therefore the SHA arithmetic and REM generated code are correct for these
probes. The original 5000-round run had been stopped by the short diagnostic
harness timeout while the slow REM emulation was still computing; it was not a
crypt correctness failure. No kernel or libc fix is justified by this probe.

## REM 0.1 login follow-up — getgroups

BusyBox's post-login test path issued syscall 158 (`getgroups`) and the
previous disposable kernel returned `ENOSYS`. The MyEmulator2 dispatcher now
routes syscall 158 to Linux's generic `sys_getgroups(int, gid_t __user *)`.
This preserves Linux pointer/count validation and returns the caller's
supplementary-group count/list semantics.

Validation:

- disposable kernel `/tmp/rem-kernel-exec/vmlinux` built successfully;
- kernel SHA-256: `9e3e2562ccc08df34b255fed9a1aff5261c77e64e6a8fbd20744d2073a127c01`;
- standalone REM probe returned `getgroups=0` rather than `ENOSYS`;
- no new syscall failure was observed in that probe.

## Targeted syscall audit — mknodat

The existing PTY/device smoke test had a concrete gap: raw syscall 33
(`mknodat`) returned `ENOSYS`, preventing userspace from creating device/FIFO
nodes even though UNIX98 PTY support itself worked. The dispatcher now routes
syscall 33 to Linux's generic `sys_mknodat`.

Validation:

- disposable kernel `/tmp/rem-kernel-exec/vmlinux` rebuilt;
- kernel SHA-256: `8b949bc36a0e158d0bc93b82010b76199300b1f7c2306c2c775dfb406ceacb31`;
- a static REM probe calling `mknod("/tmp/rem-mknod", S_IFIFO|0600, 0)` returned
  `mknod=0` in a disposable ext4 image.

No additional `ENOSYS` was exposed by the focused `getgroups`/`mknodat`
probes. Further audit should be driven by actual failures from Yash, Kilo,
BusyBox, chibicc, binutils, make/samu, or remdbg rather than the generic
Linux syscall list.

## Targeted REM 0.1 workload smoke audit

Using a disposable RC3 rootfs and the rebuilt disposable kernel, the following
startup/usage probes completed without an `ENOSYS` report:

- `chibicc --help`
- `as --version`
- `ld --version`
- `ar --version`
- `nm --version`
- `objdump --version`
- `readelf --version`
- `make --version`
- BusyBox filesystem test path

`yash -c` returned status 255 in this noninteractive launcher, so that result
is not treated as a shell acceptance result; interactive Yash acceptance is
covered separately. `samu -h` returned 130 in this launcher and requires its
own owner test rather than a kernel conclusion. No new missing syscall was
observed in the captured tool output.
