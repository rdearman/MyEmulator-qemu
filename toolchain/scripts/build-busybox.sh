#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
version=${MYEMU_BUSYBOX_VERSION:-1.37.0}
cache=${MYEMU_SOURCE_CACHE:-${TMPDIR:-/tmp}/myemulator2-sources}
archive="$cache/busybox-$version.tar.bz2"
source_dir=${MYEMU_BUSYBOX_SOURCE:-$cache/busybox-$version}
build_dir=${MYEMU_BUSYBOX_BUILD:-$root/.busybox-build}
prefix=${MYEMU_MUSL_PREFIX:-$root/.musl-install}
gcc=${MYEMU_TARGET_GCC:-$root/.toolchain-install/bin/myemulator2-elf-gcc}
cross=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install/bin/myemulator2-elf-}
cc=${MYEMU_MUSL_CC:-$root/toolchain/scripts/myemulator2-musl-gcc}

[[ "$version" == 1.37.0 ]] || { echo "only BusyBox 1.37.0 is pinned" >&2; exit 2; }
mkdir -p "$cache"
if [[ ! -f "$archive" ]]; then
	url="https://busybox.net/downloads/busybox-$version.tar.bz2"
	curl -fL "$url" -o "$archive"
fi
printf '%s  %s\n' \
	3311dff32e746499f4df0d5df04d7eb396382d7e108bb9250e7b519b837043a4 \
	"$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then
	mkdir -p "$(dirname "$source_dir")"
	tar -xf "$archive" -C "$(dirname "$source_dir")"
fi

mkdir -p "$build_dir"
if [[ ! -f "$build_dir/.config" ]]; then
	make -C "$source_dir" O="$build_dir" allnoconfig </dev/null >/dev/null
	set_config() {
		local name=$1 value=$2
		if grep -qE "^(# )?CONFIG_${name}(=| is not set)" "$build_dir/.config"; then
			sed -i -E "s@^(# )?CONFIG_${name}(=.*| is not set)@CONFIG_${name}=${value}@" \
				"$build_dir/.config"
		else
			printf 'CONFIG_%s=%s\\n' "$name" "$value" >> "$build_dir/.config"
		fi
	}
	for option in LFS STATIC STATIC_LIBGCC INSTALL_APPLET_SYMLINKS \
		SHELL_ASH ASH TRUE LS CAT ECHO PWD UNAME MKDIR TOUCH CP MV RM \
		HEAD TAIL GREP FIND PS MOUNT UMOUNT DMESG VI; do
		set_config "$option" y
	done
	set +e
	yes '' | make -C "$source_dir" O="$build_dir" oldconfig >/dev/null
	oldconfig_rc=${PIPESTATUS[1]}
	set -e
	(( oldconfig_rc == 0 ))
fi
make -C "$source_dir" O="$build_dir" \
	CC="$cc" CROSS_COMPILE="$cross" \
	MYEMU_TARGET_GCC="$gcc" MYEMU_MUSL_PREFIX="$prefix" \
	CFLAGS="-isystem$prefix/include" \
	CONFIG_EXTRA_LDFLAGS="-L$prefix/lib" \
	CONFIG_EXTRA_LDLIBS="-lc -lgcc" -j"${JOBS:-1}"
mkdir -p "$build_dir/_install/bin"
install -m 0755 "$build_dir/busybox" "$build_dir/_install/bin/busybox"
for applet in sh ls cat echo pwd uname mkdir touch cp mv rm head tail grep find ps mount umount dmesg vi; do
	ln -sf busybox "$build_dir/_install/bin/$applet"
done
printf '%s\n' "$build_dir/busybox"
