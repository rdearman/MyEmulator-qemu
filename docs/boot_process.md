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

The repository's `rikmon/rikmon.s` is a small ROM monitor. It prints a prompt
and reads a complete line into RAM at `0xef00`, with bounded input, backspace
editing, and case-insensitive commands. At reset it starts a 30-second
virtual-time autoboot countdown; pressing a key cancels it and enters the
monitor, while expiry invokes the normal raw sector-0 boot path.

The current bring-up monitor also provides these software commands:

```text
M address [value...]   examine or deposit RAM/ROM/MMIO bytes
D start [end]          dump an inclusive range
F start end value      fill an inclusive RAM range
R                      display the readable firmware register state
G address              transfer control with JA
BOOT                   read raw floppy sector 0 into 0x0200
HELP                   show the command summary
Q                      halt the CPU
```

Commands are case-insensitive. Input is edited in a bounded 63-character
buffer at `0xef00`; Backspace and Delete erase the previous character, and
the completed line is zero terminated. `BOOT` loads sector 0 at `0x0200` and
transfers control there with `JA A2`. `G` parses its address and transfers
control with `JA A2`; an odd target enters the firmware alignment handler.
`R` can display
R0-R3, A0-A3, S0, and the monitor's current SP; LR and PC are not directly
readable by firmware software and should be inspected with the native
debugger.

At reset RIKMON starts a one-shot 30,000 ms virtual-time timer. It displays
that autoboot is pending and polls both the console and timer. A key cancels
autoboot and is consumed; otherwise expiry stops the timer and enters the
same raw sector-0 `BOOT` path used by the manual command. A failed automatic
boot reports the normal boot error and returns to the monitor without retrying
until the next machine reset.

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
