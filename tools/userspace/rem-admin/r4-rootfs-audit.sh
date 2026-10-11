#!/data/data/com.termux/files/usr/bin/bash
# Resolve and log the exact R4 files selected by both launch modes.
# Usage: ./r4-rootfs-audit.sh inspect|normal|recovery
set -euo pipefail

mode="${1:-}"
case "$mode" in
  inspect|normal|recovery) ;;
  *) echo "usage: $0 inspect|normal|recovery" >&2; exit 2 ;;
esac

here="$(cd -- "$(dirname -- "$0")" && pwd -P)"
qemu="${REM_QEMU:-$here/qemu-system-myemulator32}"
kernel="${REM_KERNEL:-$here/vmlinux}"
rootfs="${REM_ROOTFS:-$here/rootfs.ext4}"
rootdir="$(cd -- "$(dirname -- "$rootfs")" && pwd -P)"
rootfs="$rootdir/$(basename -- "$rootfs")"
log="${REM_ROOTFS_AUDIT_LOG:-$HOME/rem-r4-rootfs-audit.log}"

if [[ ! -f "$rootfs" ]]; then
  echo "rootfs does not exist: $rootfs" >&2
  exit 2
fi
if ! command -v sha256sum >/dev/null 2>&1; then
  echo "sha256sum is required (install Termux coreutils)" >&2
  exit 2
fi

{
  printf '\n=== %s mode=%s ===\n' "$(date -Is 2>/dev/null || date)" "$mode"
  printf 'PWD=%s\nPACKAGE_DIR=%s\nQEMU=%s\nKERNEL=%s\nROOTFS=%s\n' \
    "$PWD" "$here" "$qemu" "$kernel" "$rootfs"
  printf 'ROOTFS_ID='
  ls -lid -- "$rootfs"
  printf 'ROOTFS_SHA256='
  sha256sum -- "$rootfs"
} | tee -a "$log"

if [[ "$mode" == inspect ]]; then
  exit 0
fi

if [[ ! -x "$qemu" || ! -r "$kernel" ]]; then
  echo "missing QEMU or kernel: $qemu $kernel" >&2
  exit 2
fi
launcher="$here/launch-rem.sh"
[[ "$mode" == recovery ]] && launcher="$here/launch-rem-admin-recovery.sh"
if [[ ! -x "$launcher" ]]; then
  echo "missing executable launcher: $launcher" >&2
  exit 2
fi

# Pin all modes to the exact paths printed above, even if the interactive
# shell had a different REM_ROOTFS/REM_KERNEL/REM_QEMU override.
exec env REM_ROOTFS="$rootfs" REM_KERNEL="$kernel" REM_QEMU="$qemu" "$launcher"
