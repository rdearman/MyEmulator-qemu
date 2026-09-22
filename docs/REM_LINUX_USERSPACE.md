# REM Linux userspace stage

The REM Linux userspace is built from pinned upstream source archives with the
project REM GCC in `.toolchain-install` and the REM musl/ncurses sysroot in
`.musl-install`. Host-side configure scripts, generators, `make`, `install`,
and `tic` remain native host tools; only target libraries and executables are
compiled with `myemulator2-elf-gcc`.

The generated stage is intentionally ignored by Git:

```sh
JOBS=2 ./toolchain/scripts/build-userspace.sh
# or
JOBS=2 make userspace-build
```

The scripts create:

```text
.userspace-downloads/  pinned source archives
.userspace-build/      extracted sources, build trees, package logs
.userspace-stage/      target root contents
```

The stage layout is rooted at `.userspace-stage` and includes `/bin`,
`/usr/bin`, `/usr/include`, `/usr/lib`, and `/usr/share/terminfo`. The sysroot
headers, musl static libraries/start files, ncurses libraries/headers, and
compiled terminfo database are copied into the stage so the tree can serve as
a standalone target development root.

The userspace batch does not rebuild or clean the completed Emacs or ncurses
outputs. It copies from `.emacs-build/target`,
`.emacs-build/emacs-30.1`, `.musl-install`, and
`.ncurses-build/stage/usr/share/terminfo` into a freshly recreated
`.userspace-stage`. The orchestrator refuses unsafe stage paths that would
overlap `.emacs-build`, `.ncurses-build`, `.musl-install`, or
`.toolchain-install`.

## Package wrappers

`toolchain/scripts/userspace-common.sh` supplies the shared fetch, SHA-256
verification, cross-configure environment, staging, and REM `readelf`
validation helpers. The per-package wrappers are:

| Package | Wrapper | Version |
| --- | --- | --- |
| sysroot/ncurses terminfo | `build-userspace-sysroot.sh` | existing `.musl-install` / ncurses 6.5 |
| zlib | `build-userspace-zlib.sh` | 1.3.1 |
| bzip2 | `build-userspace-bzip2.sh` | 1.0.8 |
| xz | `build-userspace-xz.sh` | 5.6.4 |
| GNU Readline | `build-userspace-readline.sh` | 8.2 |
| atomic compatibility runtime | `build-userspace-atomic-compat.sh` | project source |
| OpenSSL | `build-userspace-openssl.sh` | 3.4.1 |
| libevent | `build-userspace-libevent.sh` | 2.1.12-stable |
| GNU Make | `build-userspace-make.sh` | 4.4.1 |
| GNU diffutils | `build-userspace-diffutils.sh` | 3.10 |
| GNU patch | `build-userspace-patch.sh` | 2.7.6 |
| GNU tar | `build-userspace-tar.sh` | 1.35 |
| GNU grep | `build-userspace-grep.sh` | 3.11 |
| GNU sed | `build-userspace-sed.sh` | 4.9 |
| GNU gzip | `build-userspace-gzip.sh` | 1.13 |
| less | `build-userspace-less.sh` | 643 |
| SQLite CLI/library | `build-userspace-sqlite.sh` | 3.46.1 |
| Lua | `build-userspace-lua.sh` | 5.4.7 |
| Emacs executable/Lisp/data | `build-userspace-emacs.sh` | 30.1 |
| Bash | `build-userspace-bash.sh` | 5.2.37 |
| curl | `build-userspace-curl.sh` | 8.12.1 |
| OpenSSH client | `build-userspace-openssh.sh` | 9.9p2 |
| Git | `build-userspace-git.sh` | 2.48.1 |
| tmux | `build-userspace-tmux.sh` | 3.5a |
| Python | `build-userspace-python.sh` | 3.12.9 |

The top-level `build-userspace.sh` runs the wrappers in dependency order,
continues across package failures, writes `.userspace-build/logs/summary.txt`,
and exits non-zero if any package fails. Exact failing compiler/configure
output is preserved in `.userspace-build/logs/<package>.log`; the last 80
lines are also copied to `<package>.failure.txt`.

Readline is configured with `bash_cv_func_sigsetjmp=missing` because the
current REM musl port provides the normal `setjmp` entry points but not a
linkable `sigsetjmp` symbol. The wrapper performs a target link smoke check
against `libreadline.a`, ncurses, and musl before reporting success.

## Emacs runtime layout

`build-userspace-emacs.sh` requires the completed REM Emacs build at
`.emacs-build/target/src/emacs`; it does not invoke the Emacs build script or
overwrite `.emacs-build`. It stages:

```text
.userspace-stage/bin/emacs -> ../usr/bin/emacs
.userspace-stage/usr/bin/emacs
.userspace-stage/usr/share/emacs/30.1/lisp/
.userspace-stage/usr/share/emacs/30.1/etc/
.userspace-stage/usr/share/emacs/30.1/etc/DOC
.userspace-stage/usr/share/emacs/30.1/etc/charsets/
.userspace-stage/usr/share/emacs/30.1/leim/
.userspace-stage/usr/share/emacs/30.1/site-lisp/
.userspace-stage/usr/share/emacs/site-lisp/
.userspace-stage/usr/share/terminfo/
```

The `lisp`, `etc`, `etc/charsets`, and `leim` trees are copied from the
extracted Emacs 30.1 source tree; the generated `etc/DOC` runtime file is
copied from `.emacs-build/target/etc/DOC`; terminfo is copied from the
completed ncurses stage. The wrapper verifies the target executable, non-empty
Lisp and charset trees, generated `DOC`, and terminfo, then records counts and
the top-level runtime tree in `.userspace-build/logs/emacs-runtime-manifest.txt`.
Emacs `lib-src` build utilities are not staged because this cross-build uses
host-executable utilities there for the build process, not target runtime
helpers.

## Validation status

The preserved stage built all previously supported packages plus Bash,
OpenSSL, curl, OpenSSH client tools, Git, libevent, tmux, bzip2, and xz.
Python is the final priority-one package failure; its exact build boundary is
recorded below.

The build verified staged ELF executables with
`.toolchain-install/bin/myemulator2-elf-readelf`. The aggregate verifier checks
every executable file that `readelf` accepts and rejects non-REM machines,
non-ELF32 output, or interpreter segments. Per-binary headers are recorded in
`.userspace-build/logs/readelf-*.txt`; the aggregate list is
`.userspace-build/logs/stage-readelf-summary.txt`.

A staged-root development smoke link was also performed with
`MYEMU_MUSL_PREFIX=.userspace-stage/usr`, using staged SQLite, Lua, Readline,
ncurses, zlib, and musl headers/libraries. The linked smoke executable's
`readelf` output is recorded in `.userspace-build/logs/readelf-smoke.txt`.

Shell-script frontends installed by upstream packages, such as `zcat`, `zgrep`,
`egrep`, and `fgrep`, are staged but are not ELF objects. They require a target
shell at runtime.

Runtime behavior is not claimed here. The stage has not been executed under
REM Linux/QEMU, so terminal behavior, process semantics, filesystem
persistence, Emacs interactivity, and SQLite database operation remain
unverified until a REM Linux runtime test is available.

The current REM kernel defconfig disables `CONFIG_NET`. Network clients and
TLS support can therefore be cross-built and inspected, but SSH, Git network
transports, and HTTPS cannot be runtime-tested with that kernel.

Python 3.12.9 is not part of the preserved stage. Its two-stage cross-build
compiled the interpreter core and the requested static `_ssl`, `_hashlib`,
zlib, bzip2, xz, SQLite, Readline, and curses sources, then failed when
CPython attempted to link a default shared extension with the static REM
linker script supplied twice. No interpreter or partial standard-library tree
was installed, so Python is not reported as complete.

## Reproducible userspace archive

Commit all build scripts before archiving so the archive metadata identifies
the exact script revision. The archive command reads the existing stage and
does not rebuild, delete, or modify it:

```sh
toolchain/scripts/archive-userspace.sh
```

The script writes an ignored `.userspace-archives/rem-userspace-<commit>.tar.gz`
and adjacent `.metadata.txt`. Entries are sorted, numeric ownership is
normalized to root, mtimes use the build-script commit timestamp, and gzip
stores no host timestamp or filename. Permissions, symbolic links, and the
complete directory structure are retained. The metadata records the absolute
archive path, SHA-256, byte size, regular-file count, total-entry count,
symlink count, full Git commit, source-date epoch, and whether Python was
actually staged. It refuses to overwrite an existing archive.

To install into a copy of a verified ext4 image while leaving the baseline
unchanged:

```sh
baseline=/path/to/verified-rem-rootfs.img
image=/tmp/rem-rootfs-with-userspace.img
mountpoint=/tmp/rem-rootfs-with-userspace
archive=/absolute/path/from/archive-metadata.tar.gz

cp --reflink=auto --preserve=all "$baseline" "$image"
mkdir -p "$mountpoint"
sudo mount -o loop "$image" "$mountpoint"
sudo tar --numeric-owner --same-owner --same-permissions \
  --overwrite -xzf "$archive" -C "$mountpoint"
sync
sudo umount "$mountpoint"
```

Only `image`, the copy, is mounted and changed. Use a normal copy instead of
`--reflink=auto` if the filesystem does not support reflinks. Verify the
archive checksum against its metadata before mounting, and boot only the copy.
Extraction may replace paths in the copied image, such as `/bin/sh`; the
verified baseline remains byte-for-byte untouched.

## Integrating into the REM root filesystem

After the Linux kernel and BusyBox shell are ready, validate and install the
ignored stage into a directory representing the future REM root filesystem:

```sh
toolchain/scripts/validate-userspace.sh --tree .userspace-stage
toolchain/scripts/integrate-userspace.sh /path/to/rem-rootfs
toolchain/scripts/validate-userspace.sh /path/to/rem-rootfs
```

Use `--stage DIR` with the integration script when the stage is elsewhere.
The installer preflights every path before copying, creates missing parent
directories, preserves staged permissions and symlinks, reuses existing
directories without changing their contents or mode, and never overwrites an
existing non-directory path. Any conflict is reported and the operation
aborts before changing the destination.

Libtool `*.la` metadata is intentionally omitted: it is not needed at runtime
and can contain absolute paths from the build machine. The corresponding
target static archives are retained and validated.

The validator checks every ELF and every member of each static archive for
ELF32 REM machine `0xf2e2`, rejects dynamic interpreters and dependencies,
checks dangling symlinks and script interpreters, and checks Emacs's Lisp,
`etc`, generated `DOC`, charset, LEIM, site-lisp, and terminfo data. Staged
shell frontends include gzip helpers with a `/bin/bash` shebang, so a
stage-only validation is expected to report that interpreter as missing until
the root filesystem supplies a compatible shell or those optional helpers are
removed. Validation does not execute target binaries.

These commands do not modify the existing REM root filesystem image. They
prepare files only; Emacs and the other applications must be executed under
REM Linux/QEMU separately before runtime support is claimed.
