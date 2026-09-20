#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
image=${1:-${MYEMU_EXT4_IMAGE:?set MYEMU_EXT4_IMAGE or pass an image}}
qemu=${QEMU_MYEMULATOR32:-$root/.qemu-build/qemu-system-myemulator32}
linux=${MYEMU_LINUX_SOURCE:-$root/.linux-build/linux-6.12.1}
build=${MYEMU_LINUX_BUILD:-$root/.linux-build/build}

"$linux/scripts/config" --file "$build/.config" --set-str INITRAMFS_SOURCE ''
"$linux/scripts/config" --file "$build/.config" --enable EXT4_FS
"$linux/scripts/config" --file "$build/.config" --set-str CMDLINE \
	'console=myemu0,115200 earlycon=myemu32,0xf0000000 root=/dev/myemu0 rw init=/sbin/init'
"$root/toolchain/scripts/build-linux.sh"
exec "$qemu" -M myemulator32 -m 16M -kernel "$build/vmlinux" \
	-drive "file=$image,format=raw,if=none,id=myemulator2-disk" \
	-nographic -serial mon:stdio "$@"
