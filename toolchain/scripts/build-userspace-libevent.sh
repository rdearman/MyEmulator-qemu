#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=2.1.12-stable
url=https://github.com/libevent/libevent/releases/download/release-$version/libevent-$version.tar.gz
sha256=92e6de1be9ec176428fd2367677e61ceffc2ee1cb119035037a27d346b0403bb
source_dir=$(userspace_fetch_extract libevent "$version" "$url" "$sha256")
build_dir="$build_root/build/libevent-$version"

userspace_autoconf_build libevent "$source_dir" "$build_dir" \
	--disable-openssl --disable-samples --disable-libevent-regress \
	--disable-thread-support
