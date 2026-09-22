#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
version=${MYEMU_LINUX_VERSION:-6.12.1}
source="$root/.linux-build-network/linux-$version"
build="$root/.linux-build-network/build"
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}

if [[ ! -x "${CROSS_COMPILE:-$root/.toolchain-install/bin/myemulator2-elf-}gcc" ]]; then
  echo "missing MyEmulator2 cross compiler; run configure-linux-network.sh with a valid CROSS_COMPILE" >&2
  exit 2
fi

if [[ ! -f "$build/.config" ]]; then
  "$root/toolchain/scripts/configure-linux-network.sh" >/dev/null
fi
make -C "$source" O="$build" ARCH=myemulator2 \
  CROSS_COMPILE="${CROSS_COMPILE:-$root/.toolchain-install/bin/myemulator2-elf-}" \
  -j"$jobs" vmlinux
echo "$build/vmlinux"
