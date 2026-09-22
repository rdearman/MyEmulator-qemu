#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
stage=${MYEMU_USERSPACE_STAGE:-$root/.userspace-stage}
output_dir=${MYEMU_USERSPACE_ARCHIVES:-$root/.userspace-archives}
readelf=${MYEMU_TARGET_READELF:-$root/.toolchain-install/bin/myemulator2-elf-readelf}

stage=$(realpath -e "$stage")
[[ -d "$stage" ]] || { echo "userspace stage is not a directory: $stage" >&2; exit 1; }
[[ -x "$readelf" ]] || { echo "REM readelf is missing: $readelf" >&2; exit 1; }

commit=$(git -C "$root" rev-parse HEAD)
commit_time=$(git -C "$root" show -s --format=%ct "$commit")
archive="$output_dir/rem-userspace-${commit:0:12}.tar.gz"
metadata="$archive.metadata.txt"
mkdir -p "$output_dir"
[[ ! -e "$archive" && ! -e "$metadata" ]] || {
	echo "refusing to overwrite an existing archive or metadata file" >&2
	exit 1
}

(
	cd "$stage"
	tar --sort=name --format=posix \
		--pax-option=delete=atime,delete=ctime \
		--mtime="@$commit_time" --clamp-mtime \
		--owner=0 --group=0 --numeric-owner \
		-cf - .
) | gzip -n >"$archive"

file_count=$(find -P "$stage" -type f | wc -l)
entry_count=$(find -P "$stage" -mindepth 1 | wc -l)
symlink_count=$(find -P "$stage" -type l | wc -l)
size=$(stat -c %s "$archive")
checksum=$(sha256sum "$archive" | awk '{print $1}')

{
	printf 'archive=%s\n' "$archive"
	printf 'sha256=%s\n' "$checksum"
	printf 'size_bytes=%s\n' "$size"
	printf 'staged_files=%s\n' "$file_count"
	printf 'staged_entries=%s\n' "$entry_count"
	printf 'staged_symlinks=%s\n' "$symlink_count"
	printf 'git_commit=%s\n' "$commit"
	printf 'source_date_epoch=%s\n' "$commit_time"
	printf 'python_staged=%s\n' "$([[ -x "$stage/usr/bin/python3" ]] && echo yes || echo no)"
} >"$metadata"

printf '%s\n%s\n' "$archive" "$metadata"
