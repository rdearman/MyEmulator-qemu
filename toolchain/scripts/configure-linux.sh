#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
version=${MYEMU_LINUX_VERSION:-6.12.1}
source="$root/.linux-build/linux-$version"
build="$root/.linux-build/build"
cross=${CROSS_COMPILE:-$root/.toolchain-install/bin/myemulator2-elf-}
"$root/toolchain/scripts/fetch-linux.sh" >/dev/null
if [[ ! -d "$source" ]]; then
  mkdir -p "$root/.linux-build"
  tar -xf "$root/.linux-downloads/linux-$version.tar.xz" -C "$root/.linux-build"
fi
python3 "$root/toolchain/scripts/prepare-linux-source.py" "$source"
rm -rf "$build"
mkdir -p "$build"
make -C "$source" O="$build" ARCH=myemulator2 \
  CROSS_COMPILE="$cross" myemulator2_defconfig
echo "$build"
