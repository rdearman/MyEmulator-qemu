# REM Fold Samurai first-edge fault (2026-10-10)

## Finding

The Fold-style rootfs had `/usr/bin/samu` at SHA-256
`98aec69f3b5810d299045010bd5a5569bae1e3102032ddf77f2c89cf1064e15f`.
This binary's build-log format string is
`0\t0\t%ld\t%s\t%lx`, although the timestamp and hash arguments are
64-bit on REM32. In `logrecord()`, `%ld` consumes one 32-bit word; the next
`%s` therefore reads the high timestamp word as a pointer. That is the
invalid low pointer observed in `memchr` while `strnlen` formats the path.

The reported PC resolves in this exact binary to `memchr+0x78` at
`0x020367f8`; LR `0x02037a40` is the return site in `strnlen`. At the fault,
`r3=0x7fffffff`, the bound used for a `%s` conversion. The observed bad
pointer `0x43` is reproduced by a 288-second output mtime: the high 32 bits
of the nanosecond mtime are `0x43`.

The corrected qualified binary at
`tools/rem-flight-image/staging/chibicc-samurai/samu-final-qualified` has
SHA-256 `1bee2e3c9aa19fe0ece0af1eb333b41e63ba7382bff993b68d297515d9d1aa93`
and emits `%lld`/`%llx`. It passes the same forced `0x43` and `0x116`
nanosecond-mtime cases and writes a valid `.ninja_log` entry.

## Resolution and path selection

The native BearSSL builder runs `samu` through `PATH` after prepending
`$USERLAND_ROOT/bootstrap/tools/native`, the coreutils native bin directory,
`/usr/bin`, and `/bin`. Therefore a newer binary installed only under
`$HOME/.local/bin` can remain shadowed by `/usr/bin/samu`. The inspected
Fold-style ext4 image contained the old 98aec binary at `/usr/bin/samu` and
no `$HOME/.local/bin/samu`. The corrective package replaces `/usr/bin/samu`
directly and preserves the old binary as a rollback copy.

## Build-edge qualification

Using the corrected binary at `/usr/bin/samu`, the exact
`src/x509/x509_minimal_full.c` BearSSL compile edge completed on the desktop
REM QEMU with the current package kernel (`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`).
Samurai returned 0; the output was a 40,248-byte REM32 relocatable object
with SHA-256
`c4b081ff3c2e471e92060945956282e327a138f624208efe3e504102d528aba9`.
The resulting disposable ext4 image passed `e2fsck -fn`.

An earlier debug kernel (`bde7533833a49357d80b934eb2c92d6cc61c75dca50803a9a76160011b6ef94a`) separately faulted the compiler during the same source compile in `hashmap.o:match`; rerunning with the current package kernel completed. This is separate from the Samurai log-format fault and must not be attributed to the Samurai binary.

## Device qualification boundary

Update (2026-10-11): the Fold owner subsequently reported that both the
corrected `1bee2e3c...` artifact and the earlier `98aec69f...` artifact fault
at the same PC/LR on the physical Fold. The desktop one-edge pass below is not
physical-device qualification and must not be used to claim the fault is
resolved.

The desktop x86-64 reproducer passes with the corrected binary. The retained
Android/AArch64 QEMU is an Android executable using `/system/bin/linker64`;
it cannot run on this x86-64 host, and no `qemu-aarch64` runner is installed.
The earlier Fold report for the 1bee binary is not reproducible in the
desktop guest. The package therefore installs the corrected binary at the
path the native builder resolves, but one final Fold build run remains
necessary to close that host-specific discrepancy.

Package: `/tmp/REM-Fold-Samu-fixed-20261010.tar.gz`

Archive SHA-256:
`28d3acf5611134b312a3257752fd99b2c4b635f8f607e26d556b502412dada83`
