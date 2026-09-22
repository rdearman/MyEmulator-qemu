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

## Building ncursesw

Build ncurses 6.5 after musl. The script configures ncurses with native
x86-64 build tools (`BUILD_CC` and the host `tic`) while `CC`, `AR`, and
`RANLIB` target REM. It enables the wide-character static libraries and
installs only headers and libraries into `.musl-install`; it never installs
into the Linux Mint host:

```sh
JOBS=2 ./toolchain/scripts/build-ncurses-rem.sh
```

The target sysroot contains `libncursesw.a`, `libtinfo.a`, and the generated
headers. The script also compiles the standard `terminfo.src` with the native
host `tic` into `.ncurses-build/stage/usr/share/terminfo`. Copy that
`usr/share/terminfo` directory into the REM root filesystem at
`/usr/share/terminfo` (or set `TERMINFO` to an alternate directory). At
runtime the terminal's `$TERM` entry must exist there; `xterm`, `xterm-256color`,
`vt100`, and `ansi` are included.

## Building Emacs against ncursesw

The reproducible two-tree build is:

```sh
JOBS=2 ./toolchain/scripts/build-emacs-rem.sh
```

The script configures and builds native host utilities in
`.emacs-build/host`, then configures the REM target independently in
`.emacs-build/target`. It copies only host-executable build utilities
(`etags`, `ctags`, `make-docfile`, `make-fingerprint`, and `ebrowse`) into
the target build. The target link uses the REM musl startup objects followed
by `libncursesw.a`, `libtinfo.a`, libc, libm, and libgcc in static-link order.
The target prefix defaults to `/usr`, and the resulting executable is
`.emacs-build/target/src/emacs`.

The final target is verified as a static ELF32 executable with the REM
machine identifier (`readelf` reports machine value `0xf2e2`). It has not
been run under REM Linux: no QEMU executable is available in this authorized
clone, so terminal editing, saving, and persistence remain unverified.

For the REM Linux userspace stage, `toolchain/scripts/build-userspace-emacs.sh`
copies the target executable to `.userspace-stage/usr/bin/emacs`, stages the
Emacs 30.1 Lisp/data directories under `.userspace-stage/usr/share/emacs/30.1`,
and copies ncurses terminfo into `.userspace-stage/usr/share/terminfo`.

The earlier `stdckdint.h` failure was a host/target configuration mix-up.
Native configuration generates Emacs's gnulib replacement, while target
configuration uses only the REM headers. The REM linker also reports the
known `.eh_frame_hdr` and RWX-segment warnings; these do not prevent
creation of the target ELF but require runtime validation.
