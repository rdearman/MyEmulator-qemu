#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=3.10
url=https://ftp.gnu.org/gnu/diffutils/diffutils-$version.tar.xz
sha256=90e5e93cc724e4ebe12ede80df1634063c7a855692685919bfe60b556c9bd09e
source_dir=$(userspace_fetch_extract diffutils "$version" "$url" "$sha256")
build_dir="$build_root/build/diffutils-$version"

userspace_autoconf_build diffutils "$source_dir" "$build_dir" --disable-nls
userspace_link_bins cmp diff diff3 sdiff
userspace_verify_bins cmp diff diff3 sdiff
printf '%s\n' "$stage_dir/usr/bin/diff"
