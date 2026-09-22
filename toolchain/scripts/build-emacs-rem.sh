#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
source_dir=${MYEMU_EMACS_SOURCE:-$root/.emacs-build/emacs-30.1}
host_build=${MYEMU_EMACS_HOST_BUILD:-$root/.emacs-build/host}
target_build=${MYEMU_EMACS_BUILD:-$root/.emacs-build/target}
host_prefix=${MYEMU_EMACS_HOST_PREFIX:-$root/.emacs-host-install}
tool_prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
musl_prefix=${MYEMU_MUSL_PREFIX:-$root/.musl-install}
jobs=${JOBS:-2}

[[ -x "$source_dir/configure" ]] ||
	{ echo "missing Emacs source: $source_dir" >&2; exit 2; }
[[ -x "$tool_prefix/bin/myemulator2-elf-gcc" ]] ||
	{ echo "missing REM GCC under $tool_prefix" >&2; exit 2; }
[[ -f "$musl_prefix/lib/libc.a" ]] ||
	{ echo "missing REM musl sysroot under $musl_prefix" >&2; exit 2; }
[[ -f "$musl_prefix/lib/libncursesw.a" ]] ||
	{ echo "missing REM ncursesw under $musl_prefix" >&2; exit 2; }

# Emacs 30.1's config.sub does not know the project-local target name.
if ! grep -q 'myemulator2' "$source_dir/build-aux/config.sub"; then
	sed -i 's/| moxie \\/| moxie | myemulator2 \\/' \
		"$source_dir/build-aux/config.sub"
fi

rm -rf "$host_build" "$target_build"
mkdir -p "$host_build" "$target_build"

# This tree owns all native configuration headers and host-executable tools.
(
	cd "$host_build"
	"$source_dir/configure" \
		--prefix="$host_prefix" \
		--without-x \
		--without-sound \
		--without-dbus \
		--without-gsettings \
		--without-toolkit-scroll-bars \
		--with-dumping=none \
		--without-compress-install \
		--with-gnutls=ifavailable \
		--without-native-compilation
	make -C lib -j"$jobs"
	make -C lib-src -j"$jobs"
)

# Configure the REM target tree separately.  Do not copy target headers into
# the host tree or use host binaries as target installables.
(
	cd "$target_build"
	CC="$tool_prefix/bin/myemulator2-elf-gcc" \
	AR="$tool_prefix/bin/myemulator2-elf-ar" \
	RANLIB="$tool_prefix/bin/myemulator2-elf-ranlib" \
	CFLAGS="-static -ffreestanding -Wno-error=pointer-sign -isystem$musl_prefix/include" \
	CPPFLAGS="-isystem$musl_prefix/include" \
	LDFLAGS="-static -L$musl_prefix/lib" \
	LIBS="-lncursesw -ltinfo -lc -lm -lgcc" \
	"$source_dir/configure" \
		--prefix="$root/.emacs-install" \
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
)

# The target uses the wide-character ncurses terminal backend.
sed -i 's/^LIBS_TERMCAP=-lncurses$/LIBS_TERMCAP=-lncursesw -ltinfo/' \
	"$target_build/src/Makefile"
sed -i 's@/\* #undef TERMINFO_DEFINES_BC \*/@#define TERMINFO_DEFINES_BC 1@' \
	"$target_build/src/config.h"
sed -i 's/^#define HAVE_PERSONALITY_ADDR_NO_RANDOMIZE 1/\/\* #undef HAVE_PERSONALITY_ADDR_NO_RANDOMIZE \*\//' \
	"$target_build/src/config.h"

# These utilities execute during the target build, so use binaries built by
# the independent native tree.  emacsclient and hexl remain target-built.
make -C "$target_build/lib" -j"$jobs"
for name in etags ctags make-docfile make-fingerprint ebrowse; do
	cp "$host_build/lib-src/$name" "$target_build/lib-src/$name"
done

crt="$musl_prefix/lib"
ldflags="-static $crt/crt1.o $crt/crti.o -T$root/toolchain/userspace/myemulator2-user.ld -L$crt"
libs="$crt/crtn.o -lncursesw -ltinfo -lc -lm -lgcc"
make -C "$target_build/src" emacs \
	LIBS_SYSTEM="$libs" LIBS_TERMCAP= LDFLAGS="$ldflags" -j"$jobs"

printf '%s\n' "$target_build/src/emacs"
