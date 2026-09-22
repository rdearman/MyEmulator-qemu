#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=3460100
release=3.46.1
url=https://www.sqlite.org/2024/sqlite-autoconf-$version.tar.gz
sha256=67d3fe6d268e6eaddcae3727fce58fcc8e9c53869bdd07a0c61e38ddf2965071
source_dir=$(userspace_fetch_extract sqlite "$release" "$url" "$sha256" "sqlite-autoconf-$version")
build_dir="$build_root/build/sqlite-$release"

sqlite_args=(--disable-readline)
sqlite_libs="-lz -lc -lgcc"
if [[ -f "$stage_dir/usr/lib/libreadline.a" ]]; then
	sqlite_args=(--enable-readline)
	sqlite_libs="-lreadline -lncursesw -ltinfo -lz -lc -lgcc"
fi

MYEMU_USERSPACE_LIBS="$sqlite_libs" \
	userspace_autoconf_build sqlite "$source_dir" "$build_dir" \
	"${sqlite_args[@]}"
userspace_link_bins sqlite3
userspace_verify_bins sqlite3
printf '%s\n' "$stage_dir/usr/bin/sqlite3"
