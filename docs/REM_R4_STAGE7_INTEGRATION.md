# R4 Stage-7 chibicc disposable integration

Date: 2026-10-04

## Image and provenance

This integration uses the frozen native REM compiler `/tmp/chibicc-stage7`.
Its SHA-256 is `72bf5c9b519eb2a33ab5eb2e014a48d21ef661049e240b3a42000f39f423eba0`.
The binary was copied without rebuilding. The original R4 `/usr/bin/chibicc`
was left in place. In the disposable guest, the Stage-7 compiler is installed
at `/home/dev/.local/bin/chibicc-stage7`; `/home/dev/.local/bin/chibicc` is
the default command because the dev login PATH includes `~/.local/bin`.
The guest provenance record is
`/home/dev/.local/share/rem/CHIBICC-STAGE7-PROVENANCE.txt`.

The R4 base rootfs is `/tmp/REM-FLIGHT-R4-ANDROID-20261002/rootfs.ext4`,
SHA-256 `3420b0e67bd252c985839b29a7b72d3668f21b50e3aef870a25e858a0cbc946f`.
The base was not mounted or modified. Changes reside in the disposable
copy-on-write image `/tmp/rem-r4-stage7-20261004/rootfs-stage7.qcow2`, whose
backing file is that exact base rootfs. Current overlay SHA-256:
`aef9ffaec7a117d54a01a597d533611b495207aa008e0c2cbf8885f6fb0f7bb2`.
The kernel used for validation is `/tmp/REM-FLIGHT-R4-ANDROID-20261002/vmlinux`,
SHA-256 `576c1f4fc599e02554e625463d1cdc0455c3e99caff4f047c06fe3c2e537d619`.

## Qualification performed

On the disposable R4 guest, a fresh dev login selected
`/home/dev/.local/bin/chibicc`. `chibicc --help` returned 0. A trivial C
program compiled to an object (580 bytes); `ld -r` returned 0; the chibicc
driver linked the final REM executable and it returned 42. After completely
stopping that QEMU and starting the same COW image again, the default compiler
path and help command still worked, the saved executable still returned 42,
and the compiler/provenance files remained present. The base rootfs hash
remained unchanged. QEMU instances started for these checks were stopped; no
`qemu-system-myemulator32` process remained at the final check.

Linking emitted the existing linker warnings about a missing
`.note.GNU-stack` note and an RWX LOAD segment; neither prevented linking or
execution. This is Stage-7 compiler qualification, not Level-7 equivalence or
self-rebuild qualification.

## Fold Make OOM / host comparison status

The Fold's Make failure remains a kernel-confirmed OOM near 114 MiB anonymous
RSS in a guest with about 120 MiB managed RAM. Desktop-generated workloads
previously reached 1000 source plus 1000 test files without comparable memory
growth (about 1.4 MiB requested-allocation peak); those fixtures have not
reproduced the Fold workload.

For host-code comparison, the current REM target translator sources
`qemu/target/myemulator32/{cpu.c,helper.c,translate.c}` were compiled with
Clang 18 and UBSan and linked into the existing x86-64 desktop QEMU object
set. This is a target-code Clang/UBSan variant, not an Android/AArch64 QEMU
build. Its binary is
`/tmp/rem-r4-stage7-20261004/clang-ubsan/qemu-system-myemulator32-clang-ubsan`,
SHA-256 `f9cdc7c517a8fd14180214616672290f825cb9a36eac04d82c3844ce8c61cc16`.
It booted the same R4 kernel and disposable rootfs, accepted a host/guest
network transfer, and completed the Stage-7 compile/link/run checks. No UBSan
runtime diagnostic appeared. Clang static analysis of those three target
files also completed without diagnostics. These results do not reproduce or
explain the Fold-only Make memory growth.

No first divergent guest operation, root cause, or fix for the Fold OOM has
been established. The Android/AArch64 execution path has not been run on an
Android host in this investigation. Do not treat the diagnostic QEMU variant
or COW rootfs as a Fold replacement or as a qualified OOM fix.

## Next Fold discriminator artifact

To separate heap allocation from other mappings and correlate it with Make's
allocation call sites, a paired process monitor was prepared at
`tools/rem-make-diagnostics/rem-make-memory-audit.sh`. It first monitors the
unmodified `/usr/bin/make`, then the separate `make-trace` binary against the
same current directory and arguments. The monitor records RSS/data/stack,
process maps, smaps rollup, and guest memory information as the child grows;
the trace run records periodic requested-allocation totals/sites. Host
control validation passed, including a resident-memory growth process and a
log path containing spaces.

The transfer bundle is `/tmp/REM-R4-MAKE-OOM-DISCRIMINATOR-20261004.tar.gz`,
SHA-256 `f37620ca781ab5f4522574493eef6882d44c380b98bde1fea604ccdf33379853`.
It contains `make-trace` SHA-256
`6ffe6fac66c81416df1896166a2e29ee1d85d00ab63277afe70c55a5441b12d4` and
the monitor SHA-256
`d8985ce16b15f4b9ae792ce43e795f0c857fc60f772e64db0bde17d1ba521748`.
This is a diagnostic request only; it has not been run on the Fold. The
traced binary adds about 1.8 MiB BSS and may OOM earlier, so interpret it
alongside the preceding uninstrumented capture.

The target audit rechecked the sensitive arithmetic/load-store paths in
`qemu/target/myemulator32/{cpu.c,translate.c,helper.c}`. The guest is explicitly
32-bit (`TARGET_LONG_BITS=32`) and instruction operands use TCG i32 operations;
the TLB indices are masked to 10 bits and the guest shift counts are masked or
bounded. Target Clang static analysis emitted no diagnostics, and the earlier
Clang/UBSan QEMU guest run emitted no sanitizer report. No host-width or
undefined-behaviour root cause has been found. These x86-64 checks cannot
validate AArch64-generated TCG machine code.

The Fold OOM dump already establishes that the process had about 114 MiB
anonymous RSS and about 114.8 MiB total virtual size at kill. That proves
resident guest-process memory growth, but the dump lacks `/proc/<pid>/maps`,
`smaps_rollup`, and per-process page-fault samples, so it cannot distinguish a
large `[heap]` from anonymous mmap regions. Existing allocator trace results
do not contain useful periodic summaries. The new paired audit captures both
the actual Make mapping layout and a separately instrumented allocation-site
trace to discriminate A/B/C/D in one Fold login session.

No guest ELF, kernel, QEMU, rootfs, or Stage-7 compiler was changed by this
diagnostic preparation. No Fold test was initiated.
