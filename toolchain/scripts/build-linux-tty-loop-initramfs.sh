#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
out=${1:-/tmp/myemu-tty-loop-initramfs}
mkdir -p "$out/rootfs/dev"
"$prefix/bin/myemulator2-elf-as" -o "$out/linux-tty-loop.o" \
  "$root/toolchain/examples/linux-tty-loop.S"
"$prefix/bin/myemulator2-elf-ld" -Ttext=0x02000000 \
  -o "$out/linux-tty-loop.elf" "$out/linux-tty-loop.o"
install -m 0755 "$out/linux-tty-loop.elf" "$out/rootfs/init"
(
  cd "$out/rootfs"
  find . -print | cpio -o -H newc --quiet > "$out/initramfs.cpio"
)
printf '%s\n' "$out/initramfs.cpio"
