# REM flight project pack

These are complete, small utilities for an offline development session, not
blank exercises. They use portable C/POSIX interfaces except for the clearly
marked REM assembly backend in `bitcount/`. Source trees and build graphs are
self-contained; copy a project into a writable guest directory before
building.

| Project | Difficulty | Useful for | Build tools |
|---|---:|---|---|
| `linegrep/` | 1/3 | Search logs and source by fixed text | chibicc; optional samu |
| `hexscope/` | 2/3 | Inspect byte streams and ELF32 headers | chibicc, as; optional samu |
| `bitcount/` | 2/3 | Count binary data and demonstrate a C/assembly ABI | chibicc, as; optional samu |
| `memo/` | 2/3 | Keep and search an append-only flight notebook | chibicc, ar; optional samu |

## Start coding

1. Choose a project and read its local `README.md`.
2. Copy it to a writable guest directory. Keep samples beside the source.
3. Check that the image has the *matching* native `chibicc`, `as`, linker,
   C runtime, and any project-specific tool. Do not mix tuples; see
   `docs/REM_VALIDATED_RUNTIME_TUPLES.md` and
   `docs/REM_C_COMPILER_STATUS.md`.
4. Build with the listed direct `build.sh` recipe. It does not use GNU Make.
   When the matching `samu` binary is accepted into the image, use the local
   `build.ninja` as documented. The graphs are kept in source control and do
   not claim that the currently staged Samurai artifact is accepted.
5. Run the command against the included sample, compare the expected output,
   then adapt it to your own files.

The validated hosted-C chain uses REM chibicc → native `as`/`ld` → matching
CRT, musl `libc.a`, and `libgcc.a`. The assembly-only image tuple does not
include chibicc or `libgcc.a`. The current flight integration notes identify
that compiler/runtime gap explicitly; these examples become buildable when
the accepted compiler/runtime artifacts are selected into the image.

When those matching files are installed at the validated paths, configure
the compiler driver before invoking a project build:

```sh
export CHIBICC_REM_AS=/root/native/bin/as
export CHIBICC_REM_LD=/root/native/bin/ld
export CHIBICC_REM_LINKER_SCRIPT=/root/native/lib/myemulator2-user.ld
export CHIBICC_REM_CRTDIR=/usr/lib
export CHIBICC_REM_GCCLIB=/usr/lib
```

These paths follow the validated chibicc tuple. Confirm the selected image's
manifest before using them; the current assembly-only base image does not
provide the compiler or compiler runtime.

## Build-model examples

- Single C translation unit: `linegrep/build.ninja`.
- Multiple C translation units: `hexscope/build.ninja`.
- C calling a native assembly routine: `bitcount/build.ninja`.
- Static archive built with `ar` and linked into an application:
  `memo/build.ninja`.

## Portability

`linegrep`, `hexscope`, and `memo` use standard C and ordinary file I/O. They
avoid REM syscalls, terminal control sequences, and compiler-specific
extensions. `bitcount/main.c` has the same C interface on every target;
`bitcount_rem.S` is only the REM implementation, while
`bitcount_portable.c` is the x86/ARM/Linux fallback. Keep any future target
assembly behind that same function boundary.

On x86/ARM Linux, the portable projects can be built with the host compiler
without a REM toolchain: use `CC=cc ./build.sh` in `linegrep/` and
`hexscope/`, `CC=cc AR=ar ./build.sh` in `memo/`, and
`./build.sh portable` in `bitcount/`.

## Expected behavior

Each project README contains a command and exact sample output. Inputs are
local files, so no network, package manager, or remote service is needed.
The notes utility creates its own append-only data file; do not point it at
unrelated files.
