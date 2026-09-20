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

The next task is to complete this build, then compile and execute a
statically linked musl program under QEMU before attempting BusyBox.
