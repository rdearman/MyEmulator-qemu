#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 QEMU KERNEL ROOTFS OUTPUT_DIR" >&2
  exit 2
fi

qemu=$1
kernel=$2
rootfs=$3
output=$4
for path in "$qemu" "$kernel" "$rootfs"; do
  [[ -r "$path" ]] || { echo "missing input: $path" >&2; exit 2; }
done
[[ -x "$qemu" ]] || { echo "QEMU is not executable: $qemu" >&2; exit 2; }
[[ -w "$(dirname "$output")" ]] || { echo "output parent is not writable: $(dirname "$output")" >&2; exit 2; }

mkdir -p "$output"
install -m 0755 "$qemu" "$output/qemu-system-myemulator32"
install -m 0644 "$kernel" "$output/vmlinux"
install -m 0644 "$rootfs" "$output/rootfs.ext4"
install -m 0755 "$(dirname "$0")/launch-rem.sh" "$output/launch-rem.sh"
cat > "$output/README" <<'EOF'
Run in Termux with: ./launch-rem.sh
The rootfs.ext4 file is persistent and must remain writable.
EOF
printf 'Deployment package: %s\n' "$output"
