#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
qemu=${QEMU_MYEMULATOR32:-$root/.qemu-build/qemu-system-myemulator32}
kernel=${1:-$root/.linux-build/build/vmlinux}
shift || true
exec "$qemu" -M myemulator32 -m 16M -kernel "$kernel" \
  -nographic -serial mon:stdio "$@"
