# REM Linux networking (isolated implementation)

This is an opt-in networking implementation for the `REM-android` checkout.
It is intentionally separate from the existing verified Android launch
configuration and must not be copied into another Linux integration tree until
the signal, shell, and filesystem persistence regressions pass there.

## Device contract

The machine exposes QEMU's existing **virtio-net** device through the
existing **virtio-mmio** transport:

| Item | Assignment |
|---|---|
| Virtio-mmio transport | `0xf0200000-0xf02001ff` |
| Interrupt | MyEmulator2 IRQ 5, level-sensitive |
| Existing console | `0xf0000000`, IRQ 4 |
| Existing block device | `0xf0100000`, unchanged |
| QEMU backend | `-netdev user,id=net0` |

No custom Ethernet protocol or network driver is added. Linux's existing
virtio-mmio and virtio-net drivers are enabled by
`myemulator2_network_defconfig`. The kernel command line registers the
transport and requests DHCP:

```text
virtio_mmio.device=0x200@0xf0200000:5 ip=dhcp
```

## Isolated build

The networking kernel uses `.linux-build-network/`, never `.linux-build/`:

```sh
CROSS_COMPILE=/path/to/myemulator2-elf- \
  ./toolchain/scripts/configure-linux-network.sh
CROSS_COMPILE=/path/to/myemulator2-elf- \
  ./toolchain/scripts/build-linux-network.sh
```

The QEMU host build must include libslirp:

```sh
QEMU_SOURCE=/tmp/rem-android-qemu-v9.2.0 \
QEMU_BUILD=/tmp/rem-net-qemu-build \
  ./android/build-ndk-qemu-network.sh
```

The host QEMU build was independently compiled with `--enable-slirp` and
contains `virtio-mmio`, `virtio-net`, and the user-mode networking backend.
The Linux kernel has not yet been built in this checkout because the required
MyEmulator2 cross compiler is absent. Therefore DHCP, packet transmission,
DNS, TCP, HTTPS, and guest interface initialization remain unverified.

## Android test configuration

The existing `android/launch-rem.sh`, QEMU binary, kernel, and rootfs are not
modified. The separate launcher expects these sibling files:

```text
qemu-system-myemulator32-network
vmlinux-network
rootfs-network.ext4
```

Run it with:

```sh
./launch-rem-network.sh
```

It adds `-netdev user,id=net0`, which requires no root privileges, TAP device,
or Android network permission. The Android QEMU build must nevertheless be
rebuilt with libslirp support before this launcher can provide networking.

## Verification boundary

Successful host QEMU compilation and virtio device registration are verified.
The following are intentionally not claimed:

* Linux kernel discovery of the virtio-mmio device;
* DHCP lease acquisition;
* gateway ping or TCP connectivity;
* DNS or HTTPS requests;
* Android ARM64 QEMU execution;
* Galaxy Fold execution;
* persistence and shell regression results with the networking kernel.

The missing cross compiler is an independent prerequisite. No experimental
files, root filesystem, build output, or process from
`~/Development/Active/MyEmulator-qemu` was used.
