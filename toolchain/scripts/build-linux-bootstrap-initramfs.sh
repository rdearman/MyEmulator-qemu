#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
out=${1:-${MYEMU_INITRAMFS_OUT:-/tmp/myemu-initramfs}}
mkdir -p "$out/rootfs"

"$prefix/bin/myemulator2-elf-as" \
  -o "$out/linux-echo.o" "$root/toolchain/examples/linux-echo.S"
"$prefix/bin/myemulator2-elf-ld" \
	-Ttext=0x00500000 \
	-o "$out/linux-echo.elf" "$out/linux-echo.o"
install -m 0755 "$out/linux-echo.elf" "$out/rootfs/init"

(
  cd "$out/rootfs"
  find . -print | cpio -o -H newc --quiet > "$out/initramfs.cpio"
)
printf '%s\n' "$out/initramfs.cpio"
