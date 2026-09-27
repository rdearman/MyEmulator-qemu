# REM flight project starters

These small starter files are intended to be copied into a writable guest
directory and expanded into real utilities. They are architecture-neutral C
at the source level, but REM's current chibicc supports a limited integer and
pointer subset. Keep code simple: avoid floating point, variadic functions,
aggregate returns and unvalidated compiler extensions. Hosted examples need
the *matching* REM chibicc, `as`, `ld`, linker script, CRT objects, musl
headers/`libc.a`, and `libgcc.a` from one runtime tuple.

## Start and build

From the repository root, these are native tool invocations (adjust paths to
where the matching artifacts were installed in the guest):

```sh
chibicc -S -o hello.s hello.c
as -o hello.o hello.s
ld -T /usr/lib/myemulator2-user.ld -o hello /usr/lib/crt1.o \
  /usr/lib/crti.o hello.o -L/usr/lib -lc -lgcc /usr/lib/crtn.o
./hello
```

The driver's hosted link command is preferred when its runtime paths are
configured:

```sh
chibicc -o hello hello.c
```

Check `REM_C_COMPILER_STATUS.md` and `REM_VALIDATED_RUNTIME_TUPLES.md` before
using those commands. They do not imply the current flight image contains
these tools or that every starter's library calls have passed on-device.

## Starters

| File | Project direction |
|---|---|
| `hello.c` | hello plus status/argument handling |
| `args.c` | command-line inspector |
| `filecopy.c` | robust buffered file copy; short-read/write loops are TODO |
| `malloc.c` | allocator exercise with explicit invariants |
| `directory.c` | directory inventory (requires `dirent.h` in guest sysroot) |
| `clock.c` | monotonic clock report |
| `terminal.c` | line-oriented terminal utility |
| `signals.c` | signal-aware long-running tool |
| `process.c` | spawn/exec/wait design note and starter |
| `pipe.c` | producer/consumer filter skeleton |
| `asm-hello.S` | syscall-only assembly hello |
| `call-asm.c`, `call-asm.S` | C calls assembly routine |
| `call-c.c`, `call-c.S` | assembly entry calls a C routine |

Project ideas: extend filecopy into a checksumming/safe-replace tool; build a
`reminfo` system report; write a streaming grep/CSV filter; build a tiny
append-only key/value store with recovery; or make a terminal dashboard using
the capability matrix. Keep terminal control behind a small interface to ease
later x86/ARM ports.

`fork()` is not currently a REM syscall. `process.c` documents the available
Linux-style `clone`/`execve`/`wait4` path and intentionally leaves its wrapper
unfinished. `signals.c`, directory calls and all other hosted calls need the
runtime headers and syscall/library support described in the status docs.

## Ninja/Samurai examples

The `build.ninja.*` files show single C, multiple C, and mixed C/assembly
dependency graphs. They assume Ninja syntax and a working `samu`/Samurai. The
examples are offline materials only; consult `REM_SAMURAI_STATUS.md` for its
acceptance state and do not treat parsing or a dry run as compiler validation.
`build.sh` provides the Make-independent fallback by invoking the native
compiler, assembler and linker directly.

Select one graph as the active file when using Samurai, for example:

```sh
samu -f build.ninja.single
samu -f build.ninja.multi
samu -f build.ninja.mixed
samu -t clean -f build.ninja.single
samu -f build.ninja.single
```

The mixed example links a hosted C `main` with an assembly function. The
assembly-only hello uses `_start` and must be linked as its own executable,
not together with CRT startup that also defines the entry point.
