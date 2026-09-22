#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=1.0.8
url=https://sourceware.org/pub/bzip2/bzip2-$version.tar.gz
sha256=ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269
source_dir=$(userspace_fetch_extract bzip2 "$version" "$url" "$sha256")
build_dir="$build_root/build/bzip2-$version"

userspace_require_target
rm -rf "$build_dir"
cp -a "$source_dir" "$build_dir"
(
	cd "$build_dir"
	userspace_set_cross_env
	make -j"$jobs" CC="$CC" AR="$AR" RANLIB="$RANLIB" \
		CFLAGS="$CPPFLAGS $CFLAGS" LDFLAGS="$LDFLAGS" \
		bzip2 bzip2recover libbz2.a
	make PREFIX="$stage_dir/usr" install
)
userspace_link_bins bunzip2 bzcat bzip2 bzip2recover
userspace_verify_bins bzip2 bzip2recover
