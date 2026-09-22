#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=1.13
url=https://ftp.gnu.org/gnu/gzip/gzip-$version.tar.xz
sha256=7454eb6935db17c6655576c2e1b0fabefd38b4d0936e0f87f48cd062ce91a057
source_dir=$(userspace_fetch_extract gzip "$version" "$url" "$sha256")
build_dir="$build_root/build/gzip-$version"

userspace_autoconf_build gzip "$source_dir" "$build_dir" --disable-nls
userspace_link_bins gzip gunzip zcat
userspace_verify_bins gzip
printf '%s\n' "$stage_dir/usr/bin/gzip"
