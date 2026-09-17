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

The old Python CLI queue and its POP bit-7 services (`SYS_EXIT`, `SYS_PRINT`,
and `SYS_UNAME`) are historical host conveniences, not current hardware. The
new architecture assigns no syscall meaning to POP or `0xf080`; the latter is
HALT. Console, storage, timers, interrupts, and banking will be designed as
real QEMU devices when their interfaces are defined.

The old `harddrive` directory, monitor commands, and direct RAM inspection are
development tooling only, not guest-visible devices.
