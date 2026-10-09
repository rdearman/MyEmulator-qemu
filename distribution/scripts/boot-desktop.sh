#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
IMAGE=${REM_DESKTOP_ROOTFS:-/tmp/rem-linux-0.2-desktop/rootfs.ext4}
QEMU=${REM_DESKTOP_QEMU:-$ROOT/REM-FLIGHT-HUMAN-UAT-FULL/qemu-system-myemulator32}
KERNEL=${REM_DESKTOP_KERNEL:-$ROOT/REM-FLIGHT-HUMAN-UAT-FULL/vmlinux}
[ -f "$IMAGE" ] || { echo "missing desktop rootfs image: $IMAGE" >&2; exit 2; }
[ -x "$QEMU" ] || { echo "missing desktop QEMU: $QEMU" >&2; exit 2; }
[ -f "$KERNEL" ] || { echo "missing REM kernel: $KERNEL" >&2; exit 2; }
exec "$QEMU" -M myemulator32 -m 128M -kernel "$KERNEL" \
    -append 'console=ttyMY0,115200 earlycon=myemulator2,0xf0000000 root=/dev/myemu0 rw init=/sbin/init virtio_mmio.device=0x200@0xf0200000:5 ip=off' \
    -drive "file=$IMAGE,format=raw,if=none,id=myemulator2-disk" \
    -nographic -monitor none -serial stdio -icount shift=0,sleep=off -no-reboot
