# RIKMON port notes

The existing `rikmon/` program is MyEmulator 1.0 firmware. It uses the 1.0
16-bit ISA, the 1.0 ROM/vector layout, byte-oriented MMIO addresses, and raw
binary loading assumptions. It must not be mechanically assembled by the
MyEmulator2 toolchain.

A future port should be a new MyEmulator2 firmware source tree. It will need:

- the MyEmulator2 GNU syntax and ABI (`r13/sp`, `r14/lr`, 16-byte stack
  alignment, and the documented caller/callee-save rules);
- a linker script placing firmware in the reserved high firmware window and
  setting the reset words at physical `0x400` and `0x404`;
- device initialization once 2.0 console/timer/block devices exist;
- an ELF32 program-header loader rather than the 1.0 raw/MyFS loader;
- filesystem code for the future block device (eventually ext4 support).

The desired future path is reset -> 2.0 RIKMON -> block device/filesystem ->
ELF32 loader -> separately linked program. No 2.0 block device or ELF loader
exists in this milestone.
