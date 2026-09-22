#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

home="${REM_HOME:-$(cd "$(dirname "$0")" && pwd)}"
qemu="${REM_QEMU:-$home/qemu-system-myemulator32}"
kernel="${REM_KERNEL:-$home/vmlinux}"
rootfs="${REM_ROOTFS:-$home/rootfs.ext4}"

[[ -x "$qemu" ]] || { echo "missing executable: $qemu" >&2; exit 2; }
[[ -r "$kernel" ]] || { echo "missing kernel: $kernel" >&2; exit 2; }
[[ -w "$rootfs" ]] || { echo "root filesystem is not writable: $rootfs" >&2; exit 2; }

# The NDK cross-build records its disposable sysroot in an ELF RUNPATH.  The
# runtime copy is supplied by Termux, so make the real device library path
# explicit before invoking the Android linker.
termux_prefix="${PREFIX:-/data/data/com.termux/files/usr}"
export LD_LIBRARY_PATH="$termux_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

exec "$qemu" \
  -M myemulator32 \
  -m 16M \
  -kernel "$kernel" \
  -drive "file=$rootfs,format=raw,if=none,id=myemulator2-disk" \
  -nographic \
  -monitor none \
  -serial stdio \
  "$@"
