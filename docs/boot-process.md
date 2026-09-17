# Boot Process

Reset obtains initial state from fixed vectors:

```text
SP = read16(0xfffc)
PC = read16(0xfffe)
```

General registers and S0 reset to zero. The vectors are little endian.

With `-kernel`, QEMU loads a development raw image into RAM at `0x0000`.
For real firmware, assemble a compact ROM image and pass it with `-bios`:

```sh
./tools/myasm rikmon.s -o rikmon.bin --firmware --debug-map rikmon.debug.json
.qemu-build/qemu-system-myemulator -M myemulator -bios rikmon.bin \
  -drive file=disk.img,format=raw,if=none,id=myemulator-floppy
```

The firmware binary represents `0xf100-0xffff`, is exactly 3840 bytes after
assembly, and defaults missing bytes to `0xff`. It owns the IRQ, SP, and reset
vectors. Reset reads SP from `0xfffc` and PC from `0xfffe`; the expected
initial SP is `0xf000`. Attaching a floppy only makes the peripheral available:
it does not install a bootstrap or change reset PC. RIKMON is responsible for
deciding whether and how to read sector 0.

The old built-in floppy bootstrap is no longer in the normal boot path.

To create and boot the example:

```sh
./tools/myasm examples/asm/floppy-sector.asm -o sector.bin --flat-64k
truncate -s 1474560 disk.img
dd if=sector.bin of=disk.img bs=256 count=1 conv=notrunc
.qemu-build/qemu-system-myemulator -M myemulator -nographic \
  -drive file=disk.img,format=raw,if=none,id=myemulator-floppy
```

The first image is:

```asm
li   r0, #2
add  r0, r0, #3
halt
```

The historical CLI loader, Intel-HEX-like format, EPROM boot prose, automatic
`PC=0`, and `jmp #0xff` stop convention are reference behaviour only.
