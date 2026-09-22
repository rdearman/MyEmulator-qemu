#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

userspace_require_target
build_dir="$build_root/build/atomic-compat"
rm -rf "$build_dir"
mkdir -p "$build_dir"
userspace_set_cross_env
"$CC" $CPPFLAGS $CFLAGS -fno-builtin -c \
	"$root/toolchain/userspace/atomic-compat.c" -o "$build_dir/atomic-compat.o"
"$AR" rcs "$stage_dir/usr/lib/libatomic_compat.a" "$build_dir/atomic-compat.o"
"$RANLIB" "$stage_dir/usr/lib/libatomic_compat.a"
printf '%s\n' "$stage_dir/usr/lib/libatomic_compat.a"
