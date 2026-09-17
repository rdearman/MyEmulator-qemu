# Devices and Peripherals

QEMU provides one CPU, flat 64 KiB RAM, and a fictional floppy controller at
`0xf000-0xf00f`. Its 256-byte sector buffer is accessed through `DATA` at
`0xf004`; see the boot documentation for the programming sequence.

The controller is backed by a raw QEMU block image supplied as:

```sh
truncate -s 1474560 mydisk.img
qemu-system-myemulator -M myemulator \
  -drive file=mydisk.img,format=raw,if=none,id=myemulator-floppy
```

The recommended initial image is 1.44 MiB (5760 logical sectors), although
the implementation uses the attached image length and supports larger raw
images. `COMMAND` values `1` and `2` read and write the selected linear sector;
`STATUS` bits are BUSY=1, READY=2, ERROR=4. Error codes are 1=no media,
2=out of range, 3=I/O error, and 4=short write.

The console is a raw byte device at `0xf010-0xf01f`; its full register map,
FIFO, IRQ4 behaviour, and chardev commands are documented in
`docs/console.md`. It is independent of the floppy controller and does not
provide prompts, echo policy, editing, or monitor commands.

The timer occupies `0xf020-0xf025` and uses QEMU guest virtual time. Its
registers are `CONTROL`, `STATUS`, `COUNT_LO`, `COUNT_HI`, `RELOAD_LO`, and
`RELOAD_HI`; the two 16-bit values are little-endian and count milliseconds.
CONTROL bits 0, 1, and 2 are ENABLE, PERIODIC, and IRQ_ENABLE. STATUS bit 0
is EXPIRED and bit 1 is RUNNING. EXPIRED is write-one-to-clear. A one-shot
timer stops at zero; a periodic timer reloads and continues. An enabled timer
asserts level-sensitive IRQ1 while EXPIRED and IRQ_ENABLE are both set.
The maximum interval is 65,535 ms. Virtual time advances while the guest
executes and stops while QEMU is paused or stopped by the debugger.

The old Python CLI queue and its POP bit-7 services (`SYS_EXIT`, `SYS_PRINT`,
and `SYS_UNAME`) are historical host conveniences, not current hardware. The
new architecture assigns no syscall meaning to POP or `0xf080`; the latter is
HALT. Storage and banking remain future hardware; the virtual timer is now a
real QEMU device at `0xf020-0xf025`.

The old `harddrive` directory, monitor commands, and direct RAM inspection are
development tooling only, not guest-visible devices.
