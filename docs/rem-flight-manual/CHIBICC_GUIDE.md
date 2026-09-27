# chibicc Guide

chibicc is the small C compiler used by the REM developer workflow. It is not a replacement for the full hosted GCC toolchain; its validated scope is the REM integer/pointer C subset.

## Compilation flow

```text
source.c
  -> preprocessing
  -> parsing
  -> AST/type analysis
  -> REM assembly generation
  -> as
  -> ld
  -> executable
```

## Basic commands

```sh
chibicc -S -o hello.s hello.c
as -o hello.o hello.s
ld -T /root/native/lib/myemulator2-user.ld -o hello hello.o
./hello
```

The driver can perform the complete supported flow:

```sh
chibicc -o hello hello.c
```

For hosted C, keep CRT objects, musl `libc.a`, `libgcc.a`, headers, and the linker script from one matching runtime tuple.

## Multiple files

```sh
chibicc -c -o main.o main.c
chibicc -c -o util.o util.c
ld -T /root/native/lib/myemulator2-user.ld -o app main.o util.o
./app
```

## Static libraries

```sh
as util.s -o util.o
ar rcs libutil.a util.o
ranlib libutil.a
ld -T /root/native/lib/myemulator2-user.ld -o app main.o -L. -lutil
```

**VERIFIED:** native chibicc, assembler, linker, CRT, musl, and libgcc have a validated hello-program tuple. **PLANNED:** self-compilation and full C11 coverage remain future milestones.
