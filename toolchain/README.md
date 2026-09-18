# MyEmulator2 GNU binutils

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

This is a bare-metal/static toolchain. Linux, GCC, dynamic linking, ext4,
RIKMON porting, and the block device are not implemented here.
