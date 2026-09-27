# REM flight project pack

The offline source pack is in [`toolchain/examples/flight-project-pack/`](../toolchain/examples/flight-project-pack/README.md).
Copy a project to a writable guest directory before building. Each project
has its own source, `README.md`, sample input, Make-independent build script,
Ninja graph, and expected output. The direct build scripts do not depend on
GNU Make; the Ninja graphs can be used by `samu` when an accepted build is
installed.

## Projects

| Project | Difficulty | Concepts exercised | Dependencies |
|---|---:|---|---|
| [linegrep](../toolchain/examples/flight-project-pack/linegrep/README.md) | 1/3 | CLI parsing, streaming input, dynamically grown lines, literal and ASCII case-insensitive search, status codes | C stdio/stdlib/string; chibicc; optional `samu` |
| [hexscope](../toolchain/examples/flight-project-pack/hexscope/README.md) | 2/3 | Multiple C files, byte-oriented file I/O, offsets, printable columns, endian-safe ELF32 header decoding | C stdio/string; chibicc, native `as`/`ld`; optional `samu` |
| [bitcount](../toolchain/examples/flight-project-pack/bitcount/README.md) | 2/3 | C calling assembly, caller-saved registers, incremental binary processing, portable implementation boundary | chibicc, native `as`/`ld`; target REM assembly or portable C; optional `samu` |
| [memo](../toolchain/examples/flight-project-pack/memo/README.md) | 2/3 | Multiple C files, static archives, escaped line format, append-only data, substring filtering, malformed-input handling | chibicc, `ar`, native `as`/`ld`, matching CRT/musl/libgcc; optional `samu` |

`linegrep` is the single-file C example. `hexscope` is the multi-file C
example. `bitcount` calls a MyEmulator2 GAS routine through a C function
boundary and includes a portable C implementation for other targets. `memo`
builds `libnote.a` and links an application against it.

## Build requirements and status

Hosted REM C links must use a single validated runtime tuple: native chibicc,
assembler/linker, CRT, musl, and the matching `libgcc.a`. See
[`REM_C_COMPILER_STATUS.md`](REM_C_COMPILER_STATUS.md) and
[`REM_VALIDATED_RUNTIME_TUPLES.md`](REM_VALIDATED_RUNTIME_TUPLES.md). The
current baseline image builder's selected tuple does not include a compiler
or `libgcc.a`; these projects are ready for integration when those artifacts
are accepted and selected. `memo` additionally needs native `ar`.

`samu` acceptance is tracked separately in [`REM_SAMURAI_STATUS.md`](REM_SAMURAI_STATUS.md).
The presence of a `build.ninja` does not mean that Samurai is installed or
accepted. Each project also supplies a direct `build.sh` path, which invokes
the compiler driver and native tools without GNU Make.

## Starting a project

From the project directory, run `./build.sh`. `linegrep`, `hexscope`, and
`memo` accept `CC=...`; `bitcount/build.sh rem` accepts `CC=... AS=...`, and
`bitcount/build.sh portable` accepts `HOSTCC=...`. Then run the sample command
shown in that project's README and compare its expected output.

All projects except the explicitly target-specific REM assembly backend use
portable C and file interfaces. They avoid requiring a graphical display,
network access, or a package manager. See the pack README for integration
and tuple guidance.

The sources live below `toolchain/examples/`, which is included in the
curated offline source contract. To add them to a later disposable flight
image, use the hash-pinned artifact overlay flow in
[`tools/rem-flight-image/README.md`](../tools/rem-flight-image/README.md)
and install them under `/root/projects/flight-project-pack/`. The production
`REM-FLIGHT-TTY` package is not part of this source-pack change.
