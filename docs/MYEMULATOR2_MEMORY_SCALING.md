# MyEmulator2 Linux RAM discovery and mapping

The MyEmulator2 machine publishes its configured RAM size as a 32-bit byte
count at physical address `0x408` during reset. Linux treats that value as the
RAM extent, registers `[0, ram_size)` with memblock (reserving the platform's
low region), and no longer substitutes a guessed 16 MiB size when the handoff
is missing. A missing or unusable descriptor is a boot-protocol error.

## Address-space layout

| Virtual range | Use |
| --- | --- |
| `0x00000000–0x3fffffff` | Userspace (`TASK_SIZE` 1 GiB) |
| `0x40000000–0x7fffffff` | vmalloc and fixed MMIO aliases |
| `0x80000000–0xffffffff` | RAM direct map, `virtual = PAGE_OFFSET + physical` |

The low identity map is temporary in purpose, not a RAM-size limit: its end
is calculated from the linked kernel image plus one page-table allocation
window. Its PTE pages and the RAM-sized direct-map PTE pages are allocated
from memblock inside that early identity-mapped window. The direct-map PTE
page count is calculated from the RAM descriptor at boot. Normal page-backed
kernel memory is accessed through the direct map; the identity mapping only
covers early kernel execution and boot allocations.

The UART and block-controller bus addresses remain unchanged. Their kernel
virtual aliases are in the vmalloc range and are installed in the master page
table. Per-process page tables store physical page-table addresses, and PTBR
is loaded with the physical address of the PGD.

## Verification

The isolated kernel output and test artifacts for this refactor are under
`.rem-memory-scale-20260925/`. Run the boot matrix with:

```sh
python3 toolchain/scripts/test-myemulator2-memory-scaling.py \
  --qemu .qemu-build/qemu-system-myemulator32 \
  --kernel .rem-memory-scale-20260925/build/vmlinux \
  --rootfs .rem-flight-build-20260924/shutdown-fix/auto-sync-rootfs.ext4 \
  --output .rem-memory-scale-20260925/matrix \
  --memory 32M 48M 64M 128M 256M 512M 1G 2G
```

Each run uses a QEMU snapshot, checks the discovered RAM range and Linux page
count, waits for the BusyBox shell, runs a shell/process smoke command, and
shuts down that QEMU instance through its private QMP socket. The recorded
boot matrix passed at all eight sizes above; the command-smoke run separately
passed at 16 MiB, 32 MiB and 2 GiB. The 2 GiB boot reported 524,031 pages and
2,096,124 KiB total RAM, then reached the shell and ran the smoke command.

The current 32-bit virtual layout provides a 2 GiB contiguous direct-map
aperture. RAM sizing and page-table allocation are dynamic throughout that
range; configurations larger than the aperture would require a separate
high-memory mapping design rather than increasing a fixed PTE array.

The test kernel is
`.rem-memory-scale-20260925/build/vmlinux` (SHA-256
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`). The
tests used the existing host QEMU (SHA-256
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`) and
the verified flight rootfs as a read-only snapshot (SHA-256
`74333998c27fdc04767fc1e81405fa7857de76d2a3950701561360e85a1b1e6b`).
