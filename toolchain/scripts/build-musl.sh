#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
version=${MYEMU_MUSL_VERSION:-1.2.5}
source_cache=${MYEMU_SOURCE_CACHE:-${TMPDIR:-/tmp}/myemulator2-sources}
archive="$source_cache/musl-$version.tar.gz"
source_dir=${MYEMU_MUSL_SOURCE:-$source_cache/musl-$version}
build_dir=${MYEMU_MUSL_BUILD:-$root/.musl-build}
prefix=${MYEMU_MUSL_PREFIX:-$root/.musl-install}
toolchain=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install/bin/myemulator2-elf-}

"$root/toolchain/scripts/fetch-musl.sh" >/dev/null
if [[ ! -d "$source_dir" ]]; then
	mkdir -p "$source_cache"
	tar -xf "$archive" -C "$source_cache"
fi
if [[ ! -d "$source_dir/arch/myemulator2" ]]; then
	mkdir -p "$source_dir/arch/myemulator2"
fi
mkdir -p "$source_dir/arch/myemulator2" "$source_dir/src/thread/myemulator2"
cp -a "$root/toolchain/musl/myemulator2/." "$source_dir/arch/myemulator2/"
if [[ -d "$root/toolchain/musl/myemulator2/src" ]]; then
	cp -a "$root/toolchain/musl/myemulator2/src/." "$source_dir/src/"
fi
# musl's release configure script has no knowledge of a new architecture.
# Patch only its target-to-ARCH table in the disposable extracted source;
# all libc implementation remains upstream and the tracked overlay supplies
# the architecture ABI.
if ! grep -q 'myemulator2\*) ARCH=myemulator2' "$source_dir/configure"; then
	sed -i 's/riscv32\*) ARCH=riscv32 ;;/riscv32*) ARCH=riscv32 ;;\nmyemulator2*) ARCH=myemulator2 ;;/' "$source_dir/configure"
fi

if [[ ! -f "$build_dir/config.mak" ]]; then
	mkdir -p "$build_dir"
	(cd "$build_dir" && CC="${CC:-${toolchain}gcc}" \
		AR="${AR:-${toolchain}ar}" RANLIB="${RANLIB:-${toolchain}ranlib}" \
		"$source_dir/configure" \
		--target=myemulator2 \
		--prefix="$prefix" \
		--disable-shared \
		--disable-wrapper)
fi
make -C "$build_dir" -j"${JOBS:-1}"
make -C "$build_dir" install
