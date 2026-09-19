# MyEmulator2 GCC cross compiler

This document describes the GCC 15.2.0 freestanding C port for the
`myemulator2-elf` target. The CPU and software ABI specifications remain the
authorities for machine and calling-convention behaviour.

## Reproducible build

Build binutils first, then build GCC from the repository root:

```sh
./toolchain/scripts/build-binutils.sh
PATH="$PWD/.toolchain-install/bin:$PATH" ./toolchain/scripts/build-gcc.sh
./toolchain/scripts/test-gcc.sh
```

The scripts use `toolchain/scripts/gcc-versions.env`, download GCC into
`.toolchain-downloads/`, build it in `.toolchain-build/gcc/`, and install the
cross compiler in `.toolchain-install/`. All three locations are ignored.
GCC's documented `contrib/download_prerequisites` helper obtains GMP, MPFR,
MPC, ISL, and gettext in the ignored source tree. No `sudo` or target libc is
required.

The resulting driver is:

```sh
.toolchain-install/bin/myemulator2-elf-gcc
```

The port is configured `--without-headers --with-newlib` and builds C and
target `libgcc` only. It is a freestanding compiler, not a hosted C library
or Linux toolchain.

## Freestanding use

The target uses the frozen register ABI: arguments in `r1`–`r4`, returns in
`r1` (or `r1:r2` for 64-bit values), `r5`–`r12` callee-saved, `r13/sp`, and
`r14/lr`. The compiler keeps `r15` as its frame-pointer register when a frame
pointer is needed; this is an implementation choice and does not change the
external ABI assignment.

The C model is 32-bit: `char` and `short` are 8 and 16 bits, `int`, `long`,
and pointers are 32 bits, and `long long` is 64 bits. The stack grows down,
has 16-byte call-boundary alignment, and has no red zone.
Outgoing stack arguments occupy the bottom of the caller frame; the saved LR
and frame-register words are placed above that outgoing area. This preserves
the frozen stack-argument rule while keeping nested calls safe.

A minimal program can be compiled to an object with:

```sh
myemulator2-elf-gcc -ffreestanding -fno-builtin -O2 -c test.c -o test.o
```

Linking requires a small startup object that defines `_start`, calls `main`,
and halts. With `-nostdlib`, compiler-generated helper routines are supplied
explicitly with `-lgcc`; this is still freestanding runtime support, not libc
or an operating system.

## Backend status

The maintained target fragments are in `toolchain/gcc/`; the preparation
script derives the surrounding GCC target plumbing from the GCC release at
build time. The backend covers the scalar integer register model, immediate
arithmetic/logical operations, shifts, multiply/divide/remainder, direct and
indirect calls, comparisons and branches, byte/halfword/word memory access,
and prologue/epilogue expansion. `LI`, calls, and returns are emitted using
the existing MyEmulator2 GNU assembler syntax.

TP is not allocated as a GPR. TLS models and dynamic TLS relocations remain
future Linux ABI work. CAS/`__atomic` lowering is also future backend work;
the CPU instruction is available to future target builtins once the Linux
atomic ABI is defined.

Aggregate-by-value, structure-return, and varargs conventions are not yet
claimed as a stable ABI. They are recorded as GCC ABI completion work until
the stack/register rules are reviewed and tested. Scalar arguments and
returns compile, but the QEMU execution regression still exposes an
unresolved call-frame/return-address bug in recursive and multi-argument
functions; those cases are not yet claimed as validated.

## GCC ABI completion proposal

The implementation-tested convention for the remaining ordinary scalar cases
is: the first four 32-bit words use `r1`–`r4`; subsequent words are laid out
in ascending 4-byte slots at the callee's incoming stack address, with no
hidden red zone. `long long` values use adjacent words in little-endian
low-word-first order. This is a GCC implementation proposal, not a replacement
for ABI v1. Structure passing/return, varargs layout, bit-fields, and full TLS
models remain future ABI review items.

The CPU CAS instruction is not yet selected by GCC's generic `__atomic`
lowering. The backend currently leaves atomic builtins to a future target
atomic pattern/runtime change; inline assembly can name `cas` directly. The
ordinary scalar C suite is present, but its end-to-end QEMU run remains a
known failing validation until the call-frame bug is corrected.

## QEMU validation

The acceptance path is GCC → existing MyEmulator2 GAS/LD → ELF32 → QEMU.
Tests should inspect the generated assembly and execute linked freestanding
fixtures under `qemu-system-myemulator32`; a compiler pass alone is not
evidence that the backend is correct.

## Linux readiness

This compiler is a prerequisite, not a Linux port. The practical next work is
the architecture port plan in `docs/MYEMULATOR_2_LINUX_PORT_PLAN.md`, followed
by early boot, traps, MMU, timer, scheduler, initramfs, and only later block
storage/ext4. No RIKMON, libc, Linux, or filesystem code is included here.
