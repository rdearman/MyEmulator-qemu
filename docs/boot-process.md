# Boot Process

Reset obtains initial state from fixed vectors:

```text
SP = read16(0xfffc)
PC = read16(0xfffe)
```

General registers and flags reset to zero. Vector byte order is tied to the
pending endianness decision.

For initial QEMU bring-up, `-kernel` loads a raw image at `0x0000` into flat
RAM. The machine supplies the reset vectors in the top four bytes and starts
the CPU at the vector-specified PC. This is temporary image setup, not a
complete ROM/EPROM design.

The first image is:

```asm
li   r0, #2
add  r0, r0, #3
halt
```

The historical CLI loader, Intel-HEX-like format, EPROM boot prose, automatic
`PC=0`, and `jmp #0xff` stop convention are reference behaviour only.
