#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

packages=(
	sysroot
	zlib
	readline
	make
	diffutils
	patch
	tar
	grep
	sed
	gzip
	less
	sqlite
	lua
	emacs
)

mkdir -p "$log_dir"
userspace_require_safe_stage
rm -rf "$stage_dir"
userspace_prepare_stage
: >"$log_dir/summary.txt"
failures=()

for package in "${packages[@]}"; do
	script="$root/toolchain/scripts/build-userspace-$package.sh"
	log="$log_dir/$package.log"
	printf '==> %s\n' "$package"
	if "$script" >"$log" 2>&1; then
		printf 'PASS %s\n' "$package" | tee -a "$log_dir/summary.txt"
	else
		rc=$?
		printf 'FAIL %s rc=%d log=%s\n' "$package" "$rc" "$log" | tee -a "$log_dir/summary.txt"
		tail -n 80 "$log" >"$log_dir/$package.failure.txt" || true
		failures+=("$package")
	fi
done

if ((${#failures[@]})); then
	printf 'userspace failures: %s\n' "${failures[*]}" >&2
	exit 1
fi

userspace_verify_stage_elfs
printf 'userspace stage: %s\n' "$stage_dir"
