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
if grep -Eq '^CONFIG_INITRAMFS_SOURCE="[^"]+"' "$build/.config" &&
   [[ ${MYEMU_ALLOW_TEST_INITRAMFS:-0} != 1 ]]; then
  echo "refusing kernel build: $build/.config still selects an embedded initramfs" >&2
  echo "use the tracked test harness or reconfigure the canonical kernel build before making a boot kernel" >&2
  exit 2
fi
make -C "$source" O="$build" ARCH=myemulator2 \
  CROSS_COMPILE="${CROSS_COMPILE:-$root/.toolchain-install/bin/myemulator2-elf-}" \
  -j"$jobs" vmlinux
echo "$build/vmlinux"
