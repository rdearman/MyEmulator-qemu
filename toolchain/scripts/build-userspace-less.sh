#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=643
url=https://www.greenwoodsoftware.com/less/less-$version.tar.gz
sha256=2911b5432c836fa084c8a2e68f6cd6312372c026a58faaa98862731c8b6052e8
source_dir=$(userspace_fetch_extract less "$version" "$url" "$sha256")
build_dir="$build_root/build/less-$version"

MYEMU_USERSPACE_LIBS="-lncursesw -ltinfo -lc -lgcc" \
	userspace_autoconf_build less "$source_dir" "$build_dir" \
	--with-regex=posix --with-secure
userspace_link_bins less lessecho lesskey
userspace_verify_bins less lessecho lesskey
printf '%s\n' "$stage_dir/usr/bin/less"
