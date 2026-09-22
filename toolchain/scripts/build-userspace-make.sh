#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=4.4.1
url=https://ftp.gnu.org/gnu/make/make-$version.tar.gz
sha256=dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3
source_dir=$(userspace_fetch_extract make "$version" "$url" "$sha256")
build_dir="$build_root/build/make-$version"

# GNU make 4.4.1 bundles old fallback prototypes that conflict with musl.
sed -i 's/extern char \*getenv ();/extern char *getenv (const char *);/' \
	"$source_dir/lib/fnmatch.c" "$source_dir/src/getopt.c"
sed -i 's/extern int getopt ();/extern int getopt (int, char *const *, const char *);/' \
	"$source_dir/src/getopt.h"

userspace_autoconf_build make "$source_dir" "$build_dir" --disable-nls
userspace_link_bins make
userspace_verify_bins make
printf '%s\n' "$stage_dir/usr/bin/make"
