#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
candidate=$script_dir
if [[ ${1:-} == */manifest.json || -d ${1:-} ]]; then
  candidate=$(cd "$1" && pwd -P)
  shift
fi
[[ -r "$candidate/manifest.json" ]] || { echo "candidate manifest missing: $candidate" >&2; exit 2; }
"$script_dir/verify.py" "$candidate"
exec "$candidate/qemu-system-myemulator32" \
  -M myemulator32 -m 128M -kernel "$candidate/vmlinux" \
  -drive "file=$candidate/rem-rootfs.ext4,format=raw,if=none,id=myemulator2-disk" \
  -nographic -monitor none -serial stdio -icount shift=0,sleep=off "$@"
