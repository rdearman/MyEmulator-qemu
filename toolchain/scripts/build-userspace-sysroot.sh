#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

if [[ ! -f "$musl_prefix/lib/libncursesw.a" ||
	! -d "$root/.ncurses-build/stage/usr/share/terminfo" ]]; then
	JOBS="$jobs" "$root/toolchain/scripts/build-ncurses-rem.sh"
fi

userspace_stage_sysroot
printf '%s\n' "$stage_dir"
