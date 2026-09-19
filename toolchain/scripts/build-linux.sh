#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
version=${MYEMU_LINUX_VERSION:-6.12.1}
source="$root/.linux-build/linux-$version"
build="$root/.linux-build/build"
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
python3 "$root/toolchain/scripts/prepare-linux-source.py" "$source" >/dev/null
if [[ ! -f "$build/.config" ]]; then
  "$root/toolchain/scripts/configure-linux.sh" >/dev/null
fi
make -C "$source" O="$build" ARCH=myemulator2 \
  CROSS_COMPILE="${CROSS_COMPILE:-$root/.toolchain-install/bin/myemulator2-elf-}" \
  -j"$jobs" vmlinux
echo "$build/vmlinux"
