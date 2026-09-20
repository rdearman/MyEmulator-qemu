# MyEmulator2 Linux progress

## Current checkpoint

The GCC soft-float comparison ICE is fixed in commit `f3b76de`; signed
immediate lowering and its four-level regression are in `e313b76`.  GCC
soft-fp `libgcc` support is now generated and installed by `38ccfa0`.
The backend lowers DFmode comparisons through the appropriate libgcc
comparison helper and then uses the normal MyEmulator2 integer branch
patterns. The focused comparison fixture and the GCC C suite pass at
`-O0`, `-O1`, `-O2`, and `-Os`; `make spec-test cpu32-test` also passes.

## Current blocker

The pinned musl 1.2.5 build is being resumed from the disposable tree
`/tmp/myemu-musl-build10`, using source cache
`/tmp/myemu-musl-cache10` and prefix `/tmp/myemu-musl-install10`.
The build has advanced through complex math and is currently exposing
additional Linux generic syscall constants needed by musl's complete
source set. These constants are being added to the tracked MyEmulator2
musl syscall header; musl is not yet a completed or runtime-verified
libc.  The complete static archive and development headers now build when
the disposable configuration suppresses a target-wchar signedness warning.
The first static musl link succeeds with GCC soft-fp helpers.  A C `main`
linked against the static musl archive and the existing minimal Linux entry
object prints `MUSL C LIBC OK` under QEMU; Linux then reports the expected
PID-1 exit panic.  The full musl `crt1` startup remains unverified: its
initial short run exposed a truncated user stack in the musl `crt_arch.h`
logical-`andi` alignment sequence, which is fixed in the working tree, but
the larger startup still needs a bounded runtime diagnosis.  BusyBox is not
yet started.

Reproduction:

```sh
cp toolchain/musl/myemulator2/bits/syscall.h.in \
  /tmp/myemu-musl-cache10/musl-1.2.5/arch/myemulator2/bits/syscall.h.in
MYEMU_SOURCE_CACHE=/tmp/myemu-musl-cache10 \
MYEMU_MUSL_BUILD=/tmp/myemu-musl-build10 \
MYEMU_MUSL_PREFIX=/tmp/myemu-musl-install10 JOBS=1 \
  ./toolchain/scripts/build-musl.sh
```

The musl architecture overlay now includes `setjmp`/`longjmp` assembly
which preserves R5-R12, SP, LR, and TP.  The static archive was rebuilt
strictly and installed at `/tmp/myemu-musl-install10`.  BusyBox 1.37.0
was cross-compiled from the disposable source tree
`/tmp/myemu-phase3/busybox-1.37.0` using a minimal applet configuration;
the resulting file `/tmp/myemu-busybox-build11/busybox` is a valid static
ELF32 MyEmulator2 executable with entry point `0x00500000` and no
undefined symbols.

The BusyBox runtime is not yet verified.  A kernel rebuild with an
initramfs containing `/init` and BusyBox reached `Run /init as init
process` and loaded `/bin/busybox`.  A 90-second no-icount run then
ended with cause 14 while the host timeout terminated QEMU; cause 14 is
the architectural NMI, so this is harness-induced rather than evidence
of a BusyBox page fault.  A longer bounded run remains dominated by the
same very slow emulation and has not reached a shell prompt.  The next
diagnostic must distinguish emulator performance from a loader/startup
stall.  The temporary boot input is `/tmp/myemu-initramfs/list`; it must
not be committed.

Reproduction of the verified build milestone:

```sh
MYEMU_SOURCE_CACHE=/tmp/myemu-musl-cache10 \
MYEMU_MUSL_BUILD=/tmp/myemu-musl-build10 \
MYEMU_MUSL_PREFIX=/tmp/myemu-musl-install10 JOBS=1 \
  ./toolchain/scripts/build-musl.sh

MYEMU_MUSL_PREFIX=/tmp/myemu-musl-install10 \
MYEMU_TARGET_GCC="$PWD/.toolchain-install/bin/myemulator2-elf-gcc" \
  make -C /tmp/myemu-phase3/busybox-1.37.0 \
  O=/tmp/myemu-busybox-build11 \
  CC="$PWD/toolchain/scripts/myemulator2-musl-gcc" \
  CROSS_COMPILE="$PWD/.toolchain-install/bin/myemulator2-elf-" \
  CONFIG_EXTRA_LDLIBS='-lc -lgcc' \
  LDFLAGS='-static -L/tmp/myemu-musl-install10/lib -Wl,-Ttext=0x00500000' \
  SKIP_STRIP=y
```

## Storage bring-up status

A first polling block controller is now implemented at MMIO base
`0xf0100000`, backed by the QEMU block layer through the named backend
`myemulator2-disk`.  It exposes a 512-byte sector interface with
identify, capacity, read, write, range, read-only, and error status
registers.  The Linux side has a synchronous blk-mq driver and the
early page tables map the controller before `mm` exists.  QEMU and
Linux both build, and Linux discovers the disposable 1 MiB backend as
major 259, minor 0.

The first probe found two real defects: the early kernel page tables
did not map the block MMIO page, and QEMU had not acquired write
permission on the backend.  Both are fixed.  A manual instrumented run
then submitted a sector-1 write and read and showed the expected first
byte and persisted the pattern in the disposable image.  The tracked
end-to-end userspace test is not yet a passing regression: in the clean
reproducible run the userspace operation currently reports
`BLOCK DATA FAIL` without a corresponding device request, indicating a
remaining Linux block-device/cache or initramfs integration issue.
Storage, ext4, and disk boot must therefore remain unclaimed until
that test passes.

The immediate next task is to isolate that userspace block operation,
then return to the BusyBox startup/runtime timeout and add the
reproducible BusyBox configuration and integration test.
