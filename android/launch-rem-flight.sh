#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
home="$(cd "$(dirname "$0")/.." && pwd)"
config="$home/config/rem.conf"
if [[ -r "$config" ]]; then
  # Values in the package config are defaults; caller environment wins.
  source "$config"
fi
qemu="$home/qemu/qemu-system-myemulator32"
kernel="$home/kernel/vmlinux"
rootfs="$home/rootfs/rem-rootfs.ext4"
for path in "$qemu" "$kernel" "$rootfs"; do
  [[ -r "$path" ]] || { echo "missing package artifact: $path" >&2; exit 2; }
done
[[ -x "$qemu" ]] || { echo "QEMU is not executable: $qemu" >&2; exit 2; }
prefix="${PREFIX:-/data/data/com.termux/files/usr}"
export LD_LIBRARY_PATH="$home/qemu/lib:$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$qemu" \
  -M "${REM_MACHINE:-myemulator32}" \
  -m "${REM_MEMORY:-128M}" \
  -kernel "$kernel" \
  -drive "file=$rootfs,format=${REM_DISK_FORMAT:-raw},if=none,id=myemulator2-disk" \
  -nographic \
  -monitor none \
  -serial "${REM_SERIAL:-stdio}" \
  -icount "${REM_ICOUNT:-shift=0,sleep=off}" \
  "$@"
