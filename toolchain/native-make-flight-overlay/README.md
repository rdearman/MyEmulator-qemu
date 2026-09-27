# REM GNU Make compatibility overlay

This directory contains the GNU Make binary qualified inside the disposable
REM guest. For image integration, install `bin/make` as `/usr/bin/make` or
`/bin/make` and preserve its executable bit.

## Why this build differs

GNU Make 4.4.1's configure run cross-compiled its runtime probe. It still set
`USE_POSIX_SPAWN`, and that process path returned `ENOSYS` on REM. Rebuilds
must pass `--disable-posix-spawn`; the qualified binary instead uses Make's
fork/vfork path with `vfork` mapped to `fork`. The binary also disables Make's
optional `SET_STACK_SIZE`/`setrlimit` hook, which is not needed for REM's
single-threaded build workflow.

The source patch protects implicit archive-rule lookup from passing a null
filename to `strlen`. Apply `patches/0001-guard-invalid-archive-search.patch`
to GNU Make 4.4.1 before rebuilding.

## Qualified binary

- GNU Make: 4.4.1
- SHA-256: `f474110061701a845f11bee32fe09a167b8334fb088940ad33f64452a4630527`
- Statically linked REM ELF; built with the available REM cross GCC/binutils
  and musl runtime artifacts.
- Test rootfs: `/tmp/rem-make-gnu-validation-codex6/final-rootfs.ext4`
  (disposable clone; not a flight image).
- Kernel SHA-256: `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`
- QEMU SHA-256: `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`

The fixture at `toolchain/tests/native-make/` runs a real C-to-assembly-to-object-to-ELF recipe using native chibicc, `as`, and `ld`; the startup assembly checks the C routine returns 42.
