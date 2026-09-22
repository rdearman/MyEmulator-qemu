#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
version=${MYEMU_LINUX_VERSION:-6.12.1}
source="$root/.linux-build-network/linux-$version"
build="$root/.linux-build-network/build"
cross=${CROSS_COMPILE:-$root/.toolchain-install/bin/myemulator2-elf-}

if [[ ! -x "${cross}gcc" ]]; then
  echo "missing MyEmulator2 cross compiler: ${cross}gcc" >&2
  echo "build binutils/GCC first, or set CROSS_COMPILE to an isolated toolchain" >&2
  exit 2
fi

"$root/toolchain/scripts/fetch-linux.sh" >/dev/null
if [[ ! -d "$source" ]]; then
  mkdir -p "$root/.linux-build-network"
  tar -xf "$root/.linux-downloads/linux-$version.tar.xz" -C "$root/.linux-build-network"
fi
python3 "$root/toolchain/scripts/prepare-linux-source.py" "$source"
rm -rf "$build"
mkdir -p "$build"
make -C "$source" O="$build" ARCH=myemulator2 \
  CROSS_COMPILE="$cross" myemulator2_network_defconfig
echo "$build"
