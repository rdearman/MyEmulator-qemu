#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=5.4.7
url=https://www.lua.org/ftp/lua-$version.tar.gz
sha256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
source_dir=$(userspace_fetch_extract lua "$version" "$url" "$sha256")
build_dir="$build_root/build/lua-$version"

userspace_require_target
rm -rf "$build_dir"
cp -a "$source_dir" "$build_dir"
(
	cd "$build_dir/src"
	userspace_set_cross_env
	make clean
	make -j"$jobs" \
		CC="$CC" AR="$AR rcu" RANLIB="$RANLIB" \
		SYSCFLAGS="$CFLAGS -DLUA_USE_POSIX" \
		SYSLDFLAGS="$LDFLAGS" \
		SYSLIBS="-lm -lc -lgcc" \
		MYCFLAGS= MYLDFLAGS= MYLIBS= \
		lua luac liblua.a
)
make -C "$build_dir" \
	INSTALL_TOP="$stage_dir/usr" \
	INSTALL_BIN="$stage_dir/usr/bin" \
	INSTALL_INC="$stage_dir/usr/include" \
	INSTALL_LIB="$stage_dir/usr/lib" \
	INSTALL_MAN="$stage_dir/usr/share/man/man1" \
	TO_LIB="liblua.a" \
	install
userspace_link_bins lua luac
userspace_verify_bins lua luac
printf '%s\n' "$stage_dir/usr/bin/lua"
