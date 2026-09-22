#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
source_dir=${MYEMU_EMACS_SOURCE:-$root/.emacs-build/emacs-30.1}
build_dir=${MYEMU_EMACS_BUILD:-$root/.emacs-build/build}
prefix=${MYEMU_EMACS_PREFIX:-$root/.emacs-install}
tool_prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
musl_prefix=${MYEMU_MUSL_PREFIX:-$root/.musl-install}

[[ -x "$tool_prefix/bin/myemulator2-elf-gcc" ]] ||
	{ echo "missing REM GCC: $tool_prefix/bin/myemulator2-elf-gcc" >&2; exit 2; }
[[ -f "$musl_prefix/lib/libc.a" ]] ||
	{ echo "missing REM musl sysroot: $musl_prefix/lib/libc.a" >&2; exit 2; }
[[ -x "$source_dir/configure" ]] ||
	{ echo "missing Emacs source: $source_dir" >&2; exit 2; }

mkdir -p "$build_dir"
cd "$build_dir"
if ! grep -q 'myemulator2' "$source_dir/build-aux/config.sub"; then
	sed -i 's/| moxie \\/| moxie | myemulator2 \\/' \
		"$source_dir/build-aux/config.sub"
fi

CC="$tool_prefix/bin/myemulator2-elf-gcc" \
AR="$tool_prefix/bin/myemulator2-elf-ar" \
RANLIB="$tool_prefix/bin/myemulator2-elf-ranlib" \
CFLAGS="-static -ffreestanding -Wno-error=pointer-sign -isystem$musl_prefix/include" \
CPPFLAGS="-isystem$musl_prefix/include" \
LDFLAGS="-static -L$musl_prefix/lib" \
LIBS="-lc -lm -lgcc" \
emacs_cv_tputs_lib='none required' \
"$source_dir/configure" \
	--prefix="$prefix" \
	--build=x86_64-pc-linux-gnu \
	--host=myemulator2-linux-musl \
	--without-x \
	--without-sound \
	--without-dbus \
	--without-gsettings \
	--without-toolkit-scroll-bars \
	--with-dumping=none \
	--without-compress-install \
	--with-gnutls=ifavailable \
	--without-native-compilation
