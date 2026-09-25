# REM phone deployment roadmap

## Current verified point

The memory-scaled Linux kernel has booted the existing persistent flight
rootfs under the matching Linux x86_64 QEMU at 128 MiB, 512 MiB, and 2 GiB.
At all three sizes the guest reported the expected RAM/page count, reached
BusyBox ash, and passed a shell command smoke test. These tests used QEMU
snapshot overlays, preserving the source ext4 image.

The desktop package is staged separately under `REM-FLIGHT-DESKTOP/`; its
builder expands a new copy of the ext4 filesystem to 10 GiB and generates a
manifest. Do not replace the older `REM-FLIGHT/` Android package or its
verified source artifacts.

## Desktop use

Create the package in a new output directory:

```sh
REM-FLIGHT-DESKTOP/scripts/build-package.sh \
  .rem-memory-scale-20260925/build/vmlinux \
  .qemu-build/qemu-system-myemulator32 \
  REM-FLIGHT/rootfs/rem-rootfs.ext4 \
  /path/to/new/REM-FLIGHT-DESKTOP
```

The builder copies kernel/QEMU and makes a 10 GiB guest ext4 disk from the
provided image copy; it refuses to replace existing output. On Linux x86_64
with the libraries listed in `qemu/HOST-DEPENDENCIES.txt`, validate
`sha256sum -c SHA256SUMS`, then execute `scripts/boot-rem.sh`. Default guest
RAM is 128 MiB. Set `REM_MEMORY=512M` to override it. The writable disk is
persistent; back it up and never boot concurrent instances against it.

## Contents and guest workflow

The rootfs includes BusyBox ash/vi, normal shell tooling, native REM GAS/ld
wrappers, and local Markdown programming/architecture notes. The flight
workflow is `vi hello.S`, `rem-as hello.S`, `rem-ld hello.o`, `rem-run hello`.
QEMU's host libraries are supplied by the Linux distribution and enumerated
in the package, so the package is independent of the development checkout but
is not a static Linux binary bundle.

## Android and Galaxy Fold

An isolated minimum viable Termux package is staged at
`REM-FLIGHT-ANDROID-MVP/`; the older `REM-FLIGHT/` package remains unchanged.
The MVP contains the memory-scaled kernel, the full flight ext4 userspace
expanded to a 10 GiB filesystem, an ARM64 Android QEMU build, its required
Termux shared libraries, a QEMU config, a serial-console launcher, and
checksums. It boots from the ext4 block device (`/dev/myemu0`), not initramfs.

The package artifacts are:

| Artifact | SHA-256 |
| --- | --- |
| `kernel/vmlinux` | `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` |
| `rootfs/rem-rootfs.ext4` | `46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` |
| `qemu/qemu-system-myemulator32` | `a6da714c1643a22e6d295967704e762369ac04dc7bca05760972e0e4b0c9e3ac` |

The exact kernel/rootfs pair booted to BusyBox ash with the desktop QEMU at
128 MiB; the test used QEMU snapshot mode and passed the shell command smoke
test. The ARM64 executable is an Android AArch64 ELF using
`/system/bin/linker64`, and the package contains its Termux runtime libraries.
The attached Android emulator currently reports `offline`; the ARM64 binary
has not been executed on a physical Galaxy Fold. Therefore phone boot remains
the final external validation step. Termux is the current launcher; a native
tap-to-launch APK is outside this MVP.

From the extracted package directory in Termux, verify and launch with:

```sh
sha256sum -c SHA256SUMS
./scripts/launch-rem.sh
```

Guest networking, SSH, DHCP and extra Linux drivers are intentionally absent
from this MVP.

## Networking and SSH

The stable guest has `CONFIG_NET` disabled and the current machine exposes no
NIC, so guest TCP/IP, DNS, LAN/localhost SSH, and OpenSSH are not implemented
or tested. `libslirp` in desktop QEMU's host dependencies is not guest
networking. Next requires a real guest network stack, QEMU device/backend
wiring, interface configuration, TCP tests, host reachability, then OpenSSH.

## Updating and recovery

Update kernel or rootfs only in a new package copy. Regenerate checksums,
repeat RAM-size boot and persistence tests, and retain a backup of the previous
package. For rootfs changes, preserve Unix permissions and symlinks; cleanly
stop QEMU and run `e2fsck` on an offline copy.
