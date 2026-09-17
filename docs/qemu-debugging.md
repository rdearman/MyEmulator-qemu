# MyEmulator QEMU debugging

Build the local QEMU checkout and binary with:

```sh
./qemu/tools/build-myemulator.sh
```

The helper keeps generated files inside the repository in `.qemu-upstream/`
and `.qemu-build/`. Both directories are ignored by Git. `QEMU_SOURCE` and
`QEMU_BUILD` can still be used to select different paths when needed.

## Stock GDB

Start QEMU paused with a raw MyEmulator binary:

```sh
./.qemu-build/qemu-system-myemulator \
  -M myemulator -kernel program.bin -nographic \
  -S -gdb tcp::1234
```

In another terminal, use stock GDB:

```text
gdb
(gdb) target remote localhost:1234
(gdb) info registers
(gdb) stepi
(gdb) continue
```

The target supplies `qemu/gdb-xml/myemulator-core.xml` through QEMU's normal
`gdb_core_xml_file` target-description mechanism. The remote register order is
R0, R1, R2, R3, A0, A1, A2, A3, LR, SP, PC, S0. Their widths are 8, 8, 8, 8,
16, 16, 16, 16, 16, 16, 16, and 8 bits respectively.

This is a raw-binary target, so GDB does not provide source symbols or a native
MyEmulator assembler/disassembler. A GDB build that accepts a target
description with 8- and 16-bit registers can use `info registers`, `stepi`, and
`continue` exactly as shown. Many distribution and MinGW ``stock GDB`` builds
only contain the host x86 architecture; those builds reject this fictional
target description and cannot parse the variable-width `g` packet. They need a
multitarget GDB build with a compatible architecture, or a future dedicated
MyEmulator GDB port. The QEMU side now supplies the correct XML and packet;
it does not pad registers or invent fake registers to appease an incompatible
GDB architecture.

## QEMU instruction tracing

Use QEMU's built-in trace output as follows:

```sh
./.qemu-build/qemu-system-myemulator \
  -M myemulator -kernel program.bin -nographic \
  -d in_asm,cpu,nochain -D qemu.log
```

The MyEmulator target installs its disassembler through `disas_set_info`, so
`in_asm` shows mnemonics and operands for implemented instructions. Reserved
or invalid encodings are printed as `.word 0xNNNN`.

The focused debugging checks can be run with:

```sh
./tests/run-qemu-debug-tests.sh
```
