# MyEmulator2 Linux progress

## Current checkpoint

The GCC soft-float comparison ICE is fixed in commit `f3b76de`.
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
libc.

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
