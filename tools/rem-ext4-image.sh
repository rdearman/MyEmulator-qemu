#!/usr/bin/env bash
# Offline helpers for safely checking, backing up, restoring, and extracting
# files from a REM ext4 image. Never run these against a disk attached to QEMU.
set -euo pipefail

usage() {
	cat >&2 <<'EOF'
Usage:
  rem-ext4-image.sh check IMAGE
  rem-ext4-image.sh backup IMAGE BACKUP
  rem-ext4-image.sh restore BACKUP NEW_IMAGE
  rem-ext4-image.sh extract IMAGE /guest/path FILE_OR_DIRECTORY

All commands require the image to be detached from QEMU. `backup` detects a
QEMU process whose command line names IMAGE. Extraction first makes a private
copy, lets e2fsck replay/recover its journal, then reads it with debugfs.
EOF
}

fail() { echo "rem-ext4-image: $*" >&2; exit 2; }
need_file() { [[ -f "$1" ]] || fail "not a file: $1"; }
offline_check() {
	local image=$1 path pid comm cmdline
	path=$(realpath "$image")
	for proc in /proc/[0-9]*; do
		pid=${proc##*/}
		[[ -r "$proc/comm" && -r "$proc/cmdline" ]] || continue
		IFS= read -r comm < "$proc/comm" || true
		[[ "$comm" == qemu-system-* ]] || continue
		cmdline=$(tr '\0' ' ' < "$proc/cmdline" 2>/dev/null || true)
		[[ "$cmdline" != *"$path"* ]] || fail "QEMU PID $pid appears to have the image open: $path"
	done
}

case "${1:-}" in
	check)
		[[ $# == 2 ]] || { usage; exit 2; }
		need_file "$2"
		offline_check "$2"
		e2fsck -fn "$2"
		;;
	backup)
		[[ $# == 3 ]] || { usage; exit 2; }
		source=$(realpath "$2")
		need_file "$source"
		offline_check "$source"
		[[ ! -e "$3" ]] || fail "backup already exists: $3"
		mkdir -p "$(dirname "$3")"
		cp --sparse=auto --reflink=auto --preserve=mode,timestamps "$source" "$3"
		source_hash=$(sha256sum "$source" | cut -d ' ' -f 1)
		backup_hash=$(sha256sum "$3" | cut -d ' ' -f 1)
		[[ "$source_hash" == "$backup_hash" ]] || fail "backup hash mismatch"
		printf '%s  %s\n' "$backup_hash" "$(basename "$3")" > "$3.sha256"
		printf 'backup verified: %s  %s\n' "$backup_hash" "$3"
		;;
	restore)
		[[ $# == 3 ]] || { usage; exit 2; }
		need_file "$2"
		[[ ! -e "$3" ]] || fail "restore target already exists: $3"
		if [[ -f "$2.sha256" ]]; then (cd "$(dirname "$2")" && sha256sum -c "$(basename "$2").sha256"); fi
		mkdir -p "$(dirname "$3")"
		cp --sparse=auto --reflink=auto --preserve=mode,timestamps "$2" "$3"
		backup_hash=$(sha256sum "$2" | cut -d ' ' -f 1)
		restore_hash=$(sha256sum "$3" | cut -d ' ' -f 1)
		[[ "$backup_hash" == "$restore_hash" ]] || fail "restore hash mismatch"
		printf 'restore verified: %s  %s\n' "$restore_hash" "$3"
		;;
	extract)
		[[ $# == 4 ]] || { usage; exit 2; }
		need_file "$2"
		offline_check "$2"
		guest_path=$3
		[[ "$guest_path" == /* && "$guest_path" =~ ^/[A-Za-z0-9._+/-]+$ ]] || fail "guest path must be absolute and contain only letters, digits, dot, underscore, plus, slash, or hyphen"
		[[ ! -e "$4" ]] || fail "output already exists: $4"
		command -v debugfs >/dev/null || fail "debugfs is required"
		recovery_dir=$(mktemp -d "${TMPDIR:-/tmp}/rem-ext4-recover.XXXXXX")
		trap 'rm -rf "$recovery_dir"' EXIT
		cp --sparse=auto --reflink=auto "$2" "$recovery_dir/recovery.ext4"
		set +e
		e2fsck -p "$recovery_dir/recovery.ext4"
		fsck_status=$?
		set -e
		(( fsck_status <= 1 )) || fail "e2fsck could not safely recover the private copy (status $fsck_status)"
		if debugfs -R "stat $guest_path" "$recovery_dir/recovery.ext4" 2>/dev/null | grep -q 'Type: directory'; then
			mkdir -p "$4"
			debugfs -R "rdump $guest_path $4" "$recovery_dir/recovery.ext4"
		else
			mkdir -p "$(dirname "$4")"
			debugfs -R "dump $guest_path $4" "$recovery_dir/recovery.ext4"
		fi
		[[ -f "$4" || -d "$4" || -L "$4" ]] || fail "debugfs did not extract $guest_path"
		;;
	*) usage; exit 2 ;;
esac
