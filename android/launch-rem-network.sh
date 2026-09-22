#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

home="${REM_HOME:-$(cd "$(dirname "$0")" && pwd)}"
export REM_QEMU="${REM_QEMU:-$home/qemu-system-myemulator32-network}"
export REM_KERNEL="${REM_KERNEL:-$home/vmlinux-network}"
export REM_ROOTFS="${REM_ROOTFS:-$home/rootfs-network.ext4}"
exec "$home/launch-rem.sh" \
  -netdev user,id=net0 \
  "$@"
