#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=1.3.1
url=https://zlib.net/fossils/zlib-$version.tar.gz
sha256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
source_dir=$(userspace_fetch_extract zlib "$version" "$url" "$sha256")
build_dir="$build_root/build/zlib-$version"

userspace_require_target
rm -rf "$build_dir"
cp -a "$source_dir" "$build_dir"
(
	cd "$build_dir"
	userspace_set_cross_env
	export CFLAGS="$CPPFLAGS $CFLAGS"
	CHOST="$target_triplet" ./configure --static --prefix=/usr
	make -j"$jobs"
	make DESTDIR="$stage_dir" install
)
printf '%s\n' "$stage_dir/usr/lib/libz.a"
