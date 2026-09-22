#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
version=${MYEMU_NCURSES_VERSION:-6.5}
source_cache=${MYEMU_SOURCE_CACHE:-$root/.ncurses-downloads}
source_dir=${MYEMU_NCURSES_SOURCE:-$root/.ncurses-build/ncurses-$version}
build_dir=${MYEMU_NCURSES_BUILD:-$root/.ncurses-build/build}
stage_dir=${MYEMU_NCURSES_STAGE:-$root/.ncurses-build/stage}
sysroot=${MYEMU_MUSL_PREFIX:-$root/.musl-install}
tool_prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
jobs=${JOBS:-2}
archive="$source_cache/ncurses-$version.tar.gz"

mkdir -p "$source_cache" "$(dirname "$source_dir")"
if [[ ! -f "$archive" ]]; then
	curl -fL "https://invisible-island.net/archives/ncurses/ncurses-$version.tar.gz" \
		-o "$archive"
fi
if [[ ! -d "$source_dir" ]]; then
	tar -xf "$archive" -C "$(dirname "$source_dir")"
fi
[[ -x "$tool_prefix/bin/myemulator2-elf-gcc" ]] ||
	{ echo "missing REM GCC under $tool_prefix" >&2; exit 2; }
[[ -f "$sysroot/lib/libc.a" ]] ||
	{ echo "missing REM musl sysroot under $sysroot" >&2; exit 2; }

# ncurses 6.5's bundled config.sub does not know the project-local target.
if ! grep -q 'myemulator2' "$source_dir/config.sub"; then
	sed -i 's/| moxie \\/| moxie | myemulator2 \\/' "$source_dir/config.sub"
fi

rm -rf "$build_dir" "$stage_dir"
mkdir -p "$build_dir" "$stage_dir"
(
	cd "$build_dir"
	CC="$tool_prefix/bin/myemulator2-elf-gcc" \
	AR="$tool_prefix/bin/myemulator2-elf-ar" \
	RANLIB="$tool_prefix/bin/myemulator2-elf-ranlib" \
	BUILD_CC="${BUILD_CC:-gcc}" \
	BUILD_CPP="${BUILD_CPP:-cpp}" \
	BUILD_CFLAGS="${BUILD_CFLAGS:--O2}" \
	CFLAGS="-static -ffreestanding -Wno-error=pointer-sign -isystem$sysroot/include" \
	CPPFLAGS="-isystem$sysroot/include" \
	LDFLAGS="-static -L$sysroot/lib" \
	LIBS="-lc -lm -lgcc" \
	"$source_dir/configure" \
		--build=x86_64-pc-linux-gnu \
		--host=myemulator2-linux-musl \
		--prefix=/usr \
		--with-build-cc="${BUILD_CC:-gcc}" \
		--with-build-cpp="${BUILD_CPP:-cpp}" \
		--enable-widec \
		--without-shared \
		--without-cxx \
		--without-cxx-binding \
		--with-termlib=tinfo \
		--with-database=terminfo.src \
		--without-manpages
	cp "$source_dir/misc/terminfo.src" misc/terminfo.src
	make -j"$jobs"
	make install.includes install.libs DESTDIR="$stage_dir"
)

mkdir -p "$sysroot/include" "$sysroot/lib"
cp -a "$stage_dir/usr/include/." "$sysroot/include/"
cp -a "$stage_dir/usr/lib/." "$sysroot/lib/"
mkdir -p "$stage_dir/usr/share/terminfo"
tic -x -o "$stage_dir/usr/share/terminfo" "$source_dir/misc/terminfo.src"
printf '%s\n' "$stage_dir/usr/share/terminfo"
