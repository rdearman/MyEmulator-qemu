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
| GNU Readline | `build-userspace-readline.sh` | 8.2 |
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

The successful stage built all requested packages. Final package failures:
none.

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
