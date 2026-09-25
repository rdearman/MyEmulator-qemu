#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
home="$(cd "$(dirname "$0")/.." && pwd -P)"
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
case "$home" in
  /sdcard/*|/storage/*)
    echo "package is on shared Android storage, which may be noexec: $home" >&2
    echo "copy it under Termux's private \$HOME and launch it there" >&2
    exit 126
    ;;
esac
[[ -w "$rootfs" ]] || { echo "rootfs is not writable: $rootfs" >&2; exit 2; }
for library in libglib-2.0.so.0 libz.so.1 libandroid-support.so libiconv.so libpcre2-8.so; do
  [[ -r "$home/qemu/lib/$library" ]] || {
    echo "missing bundled QEMU library: $home/qemu/lib/$library" >&2
    exit 2
  }
done
prefix="${PREFIX:-/data/data/com.termux/files/usr}"
export LD_LIBRARY_PATH="$home/qemu/lib:$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if "$qemu" \
  -M "${REM_MACHINE:-myemulator32}" \
  -m "${REM_MEMORY:-128M}" \
  -kernel "$kernel" \
  -drive "file=$rootfs,format=${REM_DISK_FORMAT:-raw},if=none,id=myemulator2-disk" \
  -nographic \
  -monitor none \
  -serial "${REM_SERIAL:-stdio}" \
  -icount "${REM_ICOUNT:-shift=0,sleep=off}" \
  "$@"; then
  exit 0
else
  status=$?
  echo "REM QEMU exited with status $status; preserve the linker/console output above." >&2
  if [[ "$status" -eq 126 || "$status" -eq 127 ]]; then
    echo "Check executable permissions, bundled libraries, Termux private storage, and adb logcat AVC denials." >&2
  fi
  exit "$status"
fi
