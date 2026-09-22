#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=1.35
url=https://ftp.gnu.org/gnu/tar/tar-$version.tar.xz
sha256=4d62ff37342ec7aed748535323930c7cf94acf71c3591882b26a7ea50f3edc16
source_dir=$(userspace_fetch_extract tar "$version" "$url" "$sha256")
build_dir="$build_root/build/tar-$version"

userspace_autoconf_build tar "$source_dir" "$build_dir" \
	--disable-nls --disable-acl --disable-xattr --without-selinux
userspace_link_bins tar
userspace_verify_bins tar
printf '%s\n' "$stage_dir/usr/bin/tar"
