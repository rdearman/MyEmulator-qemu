#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=3.5a
url=https://github.com/tmux/tmux/releases/download/$version/tmux-$version.tar.gz
sha256=16216bd0877170dfcc64157085ba9013610b12b082548c7c9542cc0103198951
source_dir=$(userspace_fetch_extract tmux "$version" "$url" "$sha256")
build_dir="$build_root/build/tmux-$version"

export LIBEVENT_CFLAGS="-I$stage_dir/usr/include"
export LIBEVENT_LIBS="-L$stage_dir/usr/lib -levent"
export LIBTINFO_CFLAGS="-I$stage_dir/usr/include"
export LIBTINFO_LIBS="-L$stage_dir/usr/lib -lncursesw -ltinfo"
MYEMU_USERSPACE_LIBS="-levent -lncursesw -ltinfo -lc -lgcc" \
	userspace_autoconf_build tmux "$source_dir" "$build_dir" \
	--enable-static
userspace_link_bins tmux
userspace_verify_bins tmux
