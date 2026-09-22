#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=5.6.4
url=https://github.com/tukaani-project/xz/releases/download/v$version/xz-$version.tar.xz
sha256=829ccfe79d769748f7557e7a4429a64d06858e27e1e362e25d01ab7b931d9c95
source_dir=$(userspace_fetch_extract xz "$version" "$url" "$sha256")
build_dir="$build_root/build/xz-$version"

userspace_autoconf_build xz "$source_dir" "$build_dir" \
	--disable-doc --disable-nls --disable-scripts --disable-lzmadec \
	--disable-lzmainfo --disable-lzma-links
userspace_link_bins xz unxz xzcat lzma unlzma lzcat
userspace_verify_bins xz
