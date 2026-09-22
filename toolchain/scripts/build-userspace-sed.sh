#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=4.9
url=https://ftp.gnu.org/gnu/sed/sed-$version.tar.xz
sha256=6e226b732e1cd739464ad6862bd1a1aba42d7982922da7a53519631d24975181
source_dir=$(userspace_fetch_extract sed "$version" "$url" "$sha256")
build_dir="$build_root/build/sed-$version"

userspace_autoconf_build sed "$source_dir" "$build_dir" --disable-nls
userspace_link_bins sed
userspace_verify_bins sed
printf '%s\n' "$stage_dir/usr/bin/sed"
