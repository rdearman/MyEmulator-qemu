# GNU Emacs cross-build status

The selected release is GNU Emacs **30.1**. It is a current stable terminal
editor release and its configure system supports a separate build and host
triplet. The source archive is kept outside Git under `.emacs-downloads/`;
the extracted source and build output are under `.emacs-build/`.

## Local prerequisites

Build the REM binutils and GCC first:

```sh
./toolchain/scripts/build-binutils.sh
PATH="$PWD/.toolchain-install/bin:$PATH" JOBS=2 \
  ./toolchain/scripts/build-gcc.sh
```

Build the static REM musl sysroot. The current REM GCC/musl ABI requires the
pointer-sign diagnostic to be non-fatal in musl's wide-character source:

```sh
MYEMU_TARGET_GCC="$PWD/.toolchain-install/bin/myemulator2-elf-gcc" \
MYEMU_TOOLCHAIN_PREFIX="$PWD/.toolchain-install/bin/myemulator2-elf-" \
MYEMU_MUSL_MAKE_CFLAGS="-Wno-error=pointer-sign" \
./toolchain/scripts/build-musl.sh
```

## Configure

GNU Emacs 30.1 was configured as `myemulator2-unknown-linux-musl` with X11,
sound, desktop integrations, GnuTLS, native compilation, and image/toolkit
features disabled. The disposable extracted source's `build-aux/config.sub`
must recognize `myemulator2`; the tracked GCC preparation script already
teaches the GCC source copies this target name.

```sh
./toolchain/scripts/configure-emacs-rem.sh
```

The configure stage completes against `.musl-install`, including headers,
libc, math, pthread, Linux syscall, and terminal capability probes. Emacs's
`tputs` check is forced to the built-in termcap implementation because the
REM sysroot does not yet provide ncurses/terminfo.

## Current blocker

The full build has not completed. Emacs invokes `lib-src` host utilities such
as `etags` while the top-level cross build is using the target compiler.
Building those utilities directly with the host compiler is insufficient
because they include the target-generated `src/config.h`, and the host
compiler on this development machine does not provide `stdckdint.h`.

A complete build therefore needs a separate host-configured Emacs utility
tree (or an upstream-supported `CC_FOR_BUILD` arrangement), followed by
passing those host tools into the target build. No REM executable is claimed
working until it is booted under REM QEMU and edits and saves a file.

The independently verified milestone is a static hosted REM ELF probe linked
against the locally built musl sysroot; it reports ELF32, machine
`MyEmulator2`, and no dynamic section.
