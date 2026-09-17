# Boot Process

Reset obtains initial state from fixed vectors:

```text
SP = read16(0xfffc)
PC = read16(0xfffe)
```

General registers and S0 reset to zero. The vectors are little endian.

With `-kernel`, QEMU loads a raw image at `0x0000` into flat RAM and supplies
the reset vectors. For a disk boot, use `-drive file=disk.img,format=raw,if=none,id=myemulator-floppy`
without `-kernel`. The machine maps the floppy controller at `0xf000`, places
the built-in bootstrap ROM at `0x0180`, and sets the reset PC to `0x0180`.
The bootstrap writes sector 0 to the controller, polls READY, copies all 256
DATA bytes to `0x0100`, and branches to `0x0100`; QEMU does not preload that
sector into guest RAM.

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
