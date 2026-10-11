# REM Fold source recovery — 2026-10-11

This recovery pins the MyEmulator2 Linux overlay, MyEmulator32 QEMU overlay,
Android QEMU build options, and the existing REM userland/compiler changes in
one repository revision. Android QEMU overlays are applied from this tree;
Linux builds prepare `linux/arch/myemulator2` from this tree against the
checksum-pinned Linux 6.12.1 source archive.

## PID 1 regression found in the failed wall-clock build

The failed build used a generated `.linux-build/build/.config` with
`CONFIG_INITRAMFS_SOURCE="/tmp/myemu-initramfs/list"`. The tracked
`toolchain/scripts/test-linux-minilibc.py` creates that list and installs its
`init.elf` at `/init`. That fixture prints the observed “C minilibc reached”
and “C syscall wrappers reached” lines, then returns zero. Linux runs an
initramfs `/init` as PID 1; its normal exit produces the observed
`Attempted to kill init! exitcode=0x00000000` panic.

The clean kernel configurations retained under `.linux-build/linux-6.12.1`
and `.linux-build/emacs-dfsp-build` have an empty `CONFIG_INITRAMFS_SOURCE`.
The source regression was that kernel test harnesses left their temporary
configuration in the shared build directory, and the normal build accepted
it. Test configuration is now restored after each fixture build, and the
normal kernel build refuses a non-empty embedded initramfs unless an explicit
test-only environment flag is set. This is a build-configuration leak, not a
missing PID 1 syscall/signal source fix.

## Bootstrap metadata and QEMU networking

The wall-clock fields occupy `0x408` and `0x40c`. The existing RAM-size and
command-line fields remain part of the boot ABI at `0x410` and `0x414`; they
must not be overlaid by the clock seed. The Linux persistent-clock hook reads
only the epoch fields and leaves the MyEmulator2 monotonic clocksource intact.

Both Android ARM64 QEMU build paths explicitly enable slirp/user networking.
The MyEmulator32 machine and CPU implementation are taken from the canonical
`qemu/` overlay in this commit. The guest rootfs and Fold launcher are not
inputs to these build scripts.
