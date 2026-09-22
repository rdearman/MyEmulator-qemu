# MyEmulator2 GNU toolchain

This directory contains the reproducible integration for GNU binutils 2.46.0.
Upstream source, build trees, installation prefixes, archives, and generated
objects are deliberately kept outside tracked source directories.

The target is `myemulator2-elf`. The ABI is defined by
[`../docs/MYEMULATOR_2_ABI.md`](../docs/MYEMULATOR_2_ABI.md). The maintained
target fragments are under `toolchain/binutils/`; the external GNU source is
prepared in an ignored build tree and is never committed.

## Build

```sh
./toolchain/scripts/fetch-binutils.sh
./toolchain/scripts/build-binutils.sh
```

The default installation prefix is `.toolchain-install/bin`. Override it with
`MYEMU_TOOLCHAIN_PREFIX`. The scripts do not use `sudo`.

The source URL and SHA256 are recorded in `toolchain/scripts/versions.env`.
The scripts use GNU binutils 2.46.0 and place downloads, extracted sources,
build output, and installed tools outside tracked directories. Set
`MYEMU_TOOLCHAIN_DOWNLOADS`, `MYEMU_TOOLCHAIN_BUILD`, and
`MYEMU_TOOLCHAIN_PREFIX` to reproduce a build in another location. A normal
host needs a C compiler, make, flex, bison, texinfo, Python 3, and the usual
development headers; no target libraries are required.

The resulting directory contains:

```text
myemulator2-elf-as ld objdump objcopy readelf nm ar ranlib
```

## Assemble, link, inspect

```sh
TOOL=.toolchain-install/bin/myemulator2-elf
$TOOL-as program.s -o program.o
$TOOL-ld program.o -o program.elf
$TOOL-readelf -h -S -s -r program.elf
$TOOL-objdump -d program.elf
$TOOL-nm program.elf
$TOOL-objcopy -O binary program.elf program.bin
```

The default linker places bare-metal `.text` at `0x00100000`, then aligns
`.rodata`, `.data`, and `.bss` according to the ABI. ELF is canonical;
`objcopy -O binary` emits file-backed allocatable contents from the lowest
load address through the highest file-backed byte, fills gaps with zero, and
does not encode the entry point or reset words. A loader must use the ELF
program headers for BSS and entry-point handling.

The provisional ELF machine value is `EM_MYEMULATOR2 = 0xF2E2`; it is
project-local and not an official allocation. The target implements the six
ABI relocations: `R_MYEMU_NONE`, `R_MYEMU_32`, `R_MYEMU_BRANCH13`,
`R_MYEMU_JUMP26`, `R_MYEMU_HI20`, and `R_MYEMU_LO12`. `li`, `call`, and `ret`
are assembler pseudo-instructions, and `%hi(expr)`/`%lo(expr)` are supported
for explicit address construction.

The target defines `.word` as a 32-bit little-endian data directive and
`.short` as 16-bit, matching the 32-bit architecture. This is important when
emitting `R_MYEMU_32` data relocations.

## Run the executable under QEMU

Build the project QEMU target first. The MyEmulator2 machine accepts either
the existing raw development image or an ELF32 file with the project-local
machine ID:

```sh
qemu-system-myemulator32 -machine myemulator32 -display none \
  -kernel program.elf -S
```

For a stopped machine, connect through the existing QMP/debug facilities and
continue execution. ELF loading installs PT_LOAD segments, zeroes the RAM
image, and writes the frozen physical reset words at `0x400` and `0x404` so
CPU reset still loads SSP and the ELF entry PC architecturally. Raw `-kernel`
compatibility remains available for the existing tests.

## Test

```sh
./toolchain/scripts/test-binutils.sh
```

This exercises assembly, ELF inspection, relocations, disassembly, and the
GNU-toolchain/QEMU integration fixtures. The QEMU execution test is included
when the MyEmulator2 QEMU binary is available.

## Tests and future work

```sh
./toolchain/scripts/test-binutils.sh
```

The test suite covers the target tools, ELF headers/sections/symbols,
relocations, disassembly, multi-object linking, raw conversion, and QEMU
execution when a built `qemu-system-myemulator32` is available. RIKMON port
notes are in [`docs/RIKMON_PORT_NOTES.md`](docs/RIKMON_PORT_NOTES.md), and the
future block-device proposal is in
[`../docs/MYEMULATOR_2_BLOCK_DEVICE.md`](../docs/MYEMULATOR_2_BLOCK_DEVICE.md).

This is a bare-metal/static toolchain. Linux, dynamic linking, ext4, RIKMON
porting, and the block device are not implemented here.

## GCC cross compiler

The GCC 15.2.0 port is maintained as source fragments under
[`toolchain/gcc/`](gcc/) and is applied automatically to an ignored GCC
source checkout. Its target is the same `myemulator2-elf` triplet and it
uses the installed binutils above.

```sh
./toolchain/scripts/fetch-gcc.sh
PATH="$PWD/.toolchain-install/bin:$PATH" ./toolchain/scripts/build-gcc.sh
PATH="$PWD/.toolchain-install/bin:$PATH" \
  QEMU_MYEMULATOR32="$PWD/.qemu-build/qemu-system-myemulator32" \
  ./toolchain/scripts/test-gcc.sh
```

The compiler is freestanding-only at this stage. A typical compile/link
sequence is:

```sh
myemulator2-elf-gcc -ffreestanding -nostdlib -c program.c -o program.o
myemulator2-elf-gcc -ffreestanding -nostdlib \
  toolchain/examples/crt0.S program.o -lgcc -o program.elf
```

See [`toolchain/docs/MYEMULATOR_2_GCC.md`](docs/MYEMULATOR_2_GCC.md) for the
backend model, current limitations, and reproducible build details. GCC
tests are deliberately separate from the binutils suite; `make gcc-test`
builds QEMU if needed and runs the C suite at `-O0`, `-O1`, `-O2`, and `-Os`,
including a focused recursive/large-frame call regression.

## Linux bootstrap input fixture

The Linux port currently has a deliberately small bootstrap syscall ABI for
userspace bring-up: `read(0, buffer, count)` polls the MyEmulator2 UART and
returns one byte or `-EAGAIN`; `write(1/2, buffer, count)` emits to the same
UART, and `exit`/`exit_group` terminate the task. The real tty driver and
blocking line-discipline path remain Linux-port work.

Build a disposable initramfs containing the actual MyEmulator2 echo program
with:

```sh
./toolchain/scripts/build-linux-bootstrap-initramfs.sh /tmp/myemu-echo-initramfs
```

The script emits `/tmp/myemu-echo-initramfs/initramfs.cpio`; it does not
modify tracked images or require `sudo`.

A stack-free bootstrap shell fixture is also available. It supports the
`help`, `echo`, `uname`, `clear`, and `exit` built-ins over the same polling
ABI:

```sh
./toolchain/scripts/build-linux-bootstrap-shell-initramfs.sh /tmp/myemu-shell-initramfs
cp /tmp/myemu-shell-initramfs/rootfs/init /tmp/myemu-init
touch /tmp/myemu-initramfs/list
make -C .linux-build/linux-6.12.1 O="$PWD/.linux-build/build" \
  ARCH=myemulator2 CROSS_COMPILE="$PWD/.toolchain-install/bin/myemulator2-elf-" \
  -j4 vmlinux
```

This is a bring-up shell, not the Linux tty or a libc/BusyBox port. The
normal tty driver, blocking terminal semantics, process execution, and
filesystem-backed userspace remain future work.

With the kernel containing this shell as its forced initramfs, its serialized
interaction regression is:

```sh
python3 toolchain/scripts/test-linux-bootstrap-shell.py
```

## REM Linux userspace stage

A broader static userspace can be cross-built and staged under the ignored
`.userspace-stage` tree:

```sh
JOBS=2 ./toolchain/scripts/build-userspace.sh
```

The orchestrator uses the REM GCC in `.toolchain-install`, the REM
musl/ncurses sysroot in `.musl-install`, native host build tools, and
per-package wrappers for GNU make, diffutils, patch, tar, grep, sed, gzip,
less, SQLite, Lua, zlib, Readline, and Emacs staging. See
[`../docs/REM_LINUX_USERSPACE.md`](../docs/REM_LINUX_USERSPACE.md) for the
package versions, stage layout, validation logs, and runtime caveats.
