#!/bin/sh
# Run on the desktop while REM's qualification daemon is running.
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	echo "Usage: $0 REM_IPV4_ADDRESS [PORT]" >&2
	exit 2
fi
REMOTE=$1
PORT=${2:-18730}
RSYNC_BIN=${RSYNC_BIN:-rsync}
TMP=${TMPDIR:-/tmp}/rem-rsync-desktop-qualification-$$
trap 'rm -rf "$TMP"' 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

fail() { echo "FAIL: $*" >&2; exit 1; }
same_file() { cmp "$1" "$2" >/dev/null 2>&1 || fail "file mismatch: $1 vs $2"; }
same_metadata() {
	[ "$(stat -c '%a:%Y' "$1")" = "$(stat -c '%a:%Y' "$2")" ] || fail "mode/mtime mismatch: $1 vs $2"
}

command -v "$RSYNC_BIN" >/dev/null 2>&1 || fail "desktop rsync client not found"
mkdir -p "$TMP/backup" "$TMP/restore/nested dir" "$TMP/readback"
printf 'desktop restore fixture\n' >"$TMP/restore/restore.txt"
printf 'int restored(void) { return 7; }\n' >"$TMP/restore/nested dir/restored.c"
chmod 0600 "$TMP/restore/restore.txt" "$TMP/restore/nested dir/restored.c"
chmod 0750 "$TMP/restore" "$TMP/restore/nested dir"
touch -t 202104050607.08 "$TMP/restore/restore.txt"
touch -t 202205060708.09 "$TMP/restore/nested dir/restored.c"
touch -t 202306070809.10 "$TMP/restore/nested dir" "$TMP/restore"

URL="rsync://$REMOTE:$PORT"
echo "== Desktop pulls REM backup module =="
"$RSYNC_BIN" --contimeout=10 --timeout=30 -rtp "$URL/backup/" "$TMP/backup/" || fail "backup pull failed"
printf 'REM rsync backup fixture\n' >"$TMP/expected-readme"
printf 'int rem_backup_fixture(void) { return 42; }\n' >"$TMP/expected-main"
printf 'UTF-8 filename fixture\n' >"$TMP/expected-unicode"
same_file "$TMP/expected-readme" "$TMP/backup/README.txt"
same_file "$TMP/expected-main" "$TMP/backup/nested dir/main.c"
same_file "$TMP/expected-unicode" "$TMP/backup/café note.txt"
[ "$(stat -c '%a' "$TMP/backup/README.txt")" = 640 ] || fail "backup permissions not preserved"
[ "$(stat -c '%Y' "$TMP/backup/README.txt")" = "1577934245" ] || fail "backup timestamp not preserved"
[ "$(stat -c '%Y' "$TMP/backup/nested dir/main.c")" = "1612325106" ] || fail "nested timestamp not preserved"
[ "$(stat -c '%a' "$TMP/backup/nested dir/main.c")" = 640 ] || fail "nested permissions not preserved"
[ "$(stat -c '%Y' "$TMP/backup/café note.txt")" = "1646370367" ] || fail "UTF-8 file timestamp not preserved"
[ "$(stat -c '%a' "$TMP/backup/café note.txt")" = 640 ] || fail "UTF-8 file permissions not preserved"

echo "== Desktop restores files to REM, then reads them back =="
"$RSYNC_BIN" --contimeout=10 --timeout=30 -rtp "$TMP/restore/" "$URL/restore/" || fail "restore push failed"
"$RSYNC_BIN" --contimeout=10 --timeout=30 -rtp "$URL/restore/" "$TMP/readback/" || fail "restore readback failed"
same_file "$TMP/restore/restore.txt" "$TMP/readback/restore.txt"
same_file "$TMP/restore/nested dir/restored.c" "$TMP/readback/nested dir/restored.c"
same_metadata "$TMP/restore/restore.txt" "$TMP/readback/restore.txt"
same_metadata "$TMP/restore/nested dir/restored.c" "$TMP/readback/nested dir/restored.c"

echo "PASS: desktop/REM daemon transfer, recursive paths, UTF-8 filename, contents, timestamps, and permissions"
