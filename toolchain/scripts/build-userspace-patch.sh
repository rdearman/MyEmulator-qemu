#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=2.7.6
url=https://ftp.gnu.org/gnu/patch/patch-$version.tar.xz
sha256=ac610bda97abe0d9f6b7c963255a11dcb196c25e337c61f94e4778d632f1d8fd
source_dir=$(userspace_fetch_extract patch "$version" "$url" "$sha256")
build_dir="$build_root/build/patch-$version"

userspace_autoconf_build patch "$source_dir" "$build_dir" --disable-nls
userspace_link_bins patch
userspace_verify_bins patch
printf '%s\n' "$stage_dir/usr/bin/patch"
