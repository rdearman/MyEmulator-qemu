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
