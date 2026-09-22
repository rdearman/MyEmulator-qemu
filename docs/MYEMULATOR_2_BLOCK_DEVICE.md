# REM block-device proposal

This is a design note, not frozen architecture and not an implementation.

The recommended first device is a simple QEMU-backed, 512-byte-sector block
controller in the reserved `0xF0000000-0xF0FFFFFF` MMIO window. A candidate
layout is a 4 KiB page containing command/status, 64-bit LBA, sector count,
guest buffer address, capacity, and error registers. A command would be
`READ`, `WRITE`, or `IDENTIFY`; completion and errors would be level-visible,
with an optional interrupt line. The device must not understand ext4.

For the initial RIKMON driver, programmed transfer through a physically
addressed buffer is the simplest safe choice. A later Linux driver can use
the same register protocol with DMA only after the MMU/DMA contract is
reviewed. QEMU should use the block-backend API so raw and qcow2 images work
without guest-visible differences. Exact registers, interrupt assignment,
DMA semantics, and caching remain open for the next hardware milestone.
