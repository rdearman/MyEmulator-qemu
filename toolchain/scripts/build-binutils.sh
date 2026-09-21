#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
source "$root/toolchain/scripts/versions.env"
downloads=${MYEMU_TOOLCHAIN_DOWNLOADS:-$root/.toolchain-downloads}
build=${MYEMU_TOOLCHAIN_BUILD:-$root/.toolchain-build}
prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
target=${MYEMU_TARGET_TRIPLET:-myemulator2-elf}
archive="$downloads/binutils-$BINUTILS_VERSION.tar.xz"

[[ -f "$archive" ]] || "$root/toolchain/scripts/fetch-binutils.sh" >/dev/null
rm -rf "$build/source" "$build/build"
mkdir -p "$build"
tar -xf "$archive" -C "$build"
mv "$build/binutils-$BINUTILS_VERSION" "$build/source"

python3 "$root/toolchain/scripts/prepare-binutils-source.py" "$build/source"
mkdir -p "$build/build"
cd "$build/build"
"$build/source/configure" \
  --target="$target" \
  --prefix="$prefix" \
  --disable-nls \
  --disable-werror \
  --disable-gdb \
  --disable-gdbserver \
  --disable-sim
make -j"${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}"
# The top-level install target also descends into optional documentation and
# gprof directories that are absent in the deliberately minimal build above.
# Install the target tools explicitly; this is the complete supported
# cross-toolchain set for this milestone.
make install-binutils install-gas install-ld
echo "Installed MyEmulator2 binutils in $prefix/bin"
