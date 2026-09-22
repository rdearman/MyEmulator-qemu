#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=3.11
url=https://ftp.gnu.org/gnu/grep/grep-$version.tar.xz
sha256=1db2aedde89d0dea42b16d9528f894c8d15dae4e190b59aecc78f5a951276eab
source_dir=$(userspace_fetch_extract grep "$version" "$url" "$sha256")
build_dir="$build_root/build/grep-$version"

userspace_autoconf_build grep "$source_dir" "$build_dir" \
	--disable-nls --disable-perl-regexp
userspace_link_bins grep egrep fgrep
userspace_verify_bins grep
printf '%s\n' "$stage_dir/usr/bin/grep"
