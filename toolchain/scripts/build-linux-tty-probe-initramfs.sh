#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
out=${1:-${MYEMU_INITRAMFS_OUT:-/tmp/myemu-tty-probe-initramfs}}
mkdir -p "$out/rootfs/dev"

"$prefix/bin/myemulator2-elf-as" \
  -o "$out/linux-tty-probe.o" "$root/toolchain/examples/linux-tty-probe.S"
"$prefix/bin/myemulator2-elf-ld" \
  -Ttext=0x00500000 -o "$out/linux-tty-probe.elf" \
  "$out/linux-tty-probe.o"
install -m 0755 "$out/linux-tty-probe.elf" "$out/rootfs/init"
(
  cd "$out/rootfs"
  find . -print | cpio -o -H newc --quiet > "$out/initramfs.cpio"
)
printf '%s\n' "$out/initramfs.cpio"
