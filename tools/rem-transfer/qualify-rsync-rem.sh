#!/usr/bin/bash
# Run inside REM using the staged rsync binary and Bash.
set -eu

RSYNC_BIN=${RSYNC_BIN:-/usr/bin/rsync}
ROOT=${REM_RSYNC_TEST_ROOT:-/tmp/rem-rsync-qualification}
PORT=${REM_RSYNC_TEST_PORT:-18730}
CONF=$ROOT/rsyncd.conf
LOG=$ROOT/rsyncd.log
DAEMON_PID=
MODE=${1:-loopback}

fail() { echo "FAIL: $*" >&2; exit 1; }
same_file() {
	local changes
	changes=$("$RSYNC_BIN" -rtpnic -- "$1" "$2") || fail "rsync comparison failed: $1 vs $2"
	[ -z "$changes" ] || fail "content/metadata mismatch: $1 vs $2: $changes"
}
same_metadata() {
	same_file "$1" "$2"
}
cleanup() {
	if [ -n "$DAEMON_PID" ]; then
		kill "$DAEMON_PID" 2>/dev/null || :
		wait "$DAEMON_PID" 2>/dev/null || :
	fi
}
trap cleanup 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

echo "== REM rsync version =="
"$RSYNC_BIN" --version || fail "rsync is missing, not executable, or failed to start: $RSYNC_BIN"

case "$ROOT" in /tmp/rem-rsync-qualification|/tmp/rem-rsync-qualification-*) ;; *) fail "test root must be below /tmp/rem-rsync-qualification" ;; esac
case "$MODE" in loopback|--serve) ;; *) fail "usage: $0 [--serve]" ;; esac
rm -rf "$ROOT"
mkdir -p "$ROOT/source/nested dir" "$ROOT/restore" "$ROOT/backup-copy" \
	"$ROOT/desktop-input/nested dir" "$ROOT/restore-readback"

printf 'REM rsync backup fixture\n' >"$ROOT/source/README.txt"
printf 'int rem_backup_fixture(void) { return 42; }\n' >"$ROOT/source/nested dir/main.c"
printf 'UTF-8 filename fixture\n' >"$ROOT/source/café note.txt"
touch -t 202001020304.05 "$ROOT/source/README.txt"
touch -t 202102030405.06 "$ROOT/source/nested dir/main.c"
touch -t 202203040506.07 "$ROOT/source/café note.txt"
touch -t 202304050607.08 "$ROOT/source/nested dir" "$ROOT/source"

printf 'desktop restore fixture\n' >"$ROOT/desktop-input/restore.txt"
printf 'int restored(void) { return 7; }\n' >"$ROOT/desktop-input/nested dir/restored.c"
touch -t 202104050607.08 "$ROOT/desktop-input/restore.txt"
touch -t 202205060708.09 "$ROOT/desktop-input/nested dir/restored.c"
touch -t 202306070809.10 "$ROOT/desktop-input/nested dir" "$ROOT/desktop-input"

if [ "$MODE" = --serve ]; then LISTEN=0.0.0.0; else LISTEN=127.0.0.1; fi
cat >"$CONF" <<EOF
address = $LISTEN
port = $PORT
lock file = $ROOT/rsyncd.lock
use chroot = no
max connections = 2

[backup]
path = $ROOT/source
read only = yes
list = yes

[restore]
path = $ROOT/restore
read only = no
list = yes
EOF

if [ "$MODE" = --serve ]; then
	echo "Qualification daemon listening on $LISTEN:$PORT"
	echo "Modules: backup (read-only), restore (read/write); Ctrl-C stops it."
	echo "This test daemon has no authentication; use only on a trusted isolated network."
	exec "$RSYNC_BIN" --daemon --no-detach --config="$CONF" --port="$PORT"
fi

"$RSYNC_BIN" --daemon --no-detach --config="$CONF" --port="$PORT" >"$LOG" 2>&1 &
DAEMON_PID=$!
ready=0
for attempt in {1..20}; do
	if "$RSYNC_BIN" --contimeout=5 "rsync://127.0.0.1:$PORT/" >/dev/null 2>&1; then
		ready=1
		break
	fi
done
[ "$ready" -eq 1 ] || { cat "$LOG" >&2; fail "daemon did not become ready on loopback:$PORT"; }

echo "== REM loopback backup (recursive, contents, mtimes, permissions) =="
"$RSYNC_BIN" --contimeout=5 --timeout=20 -rtp "rsync://127.0.0.1:$PORT/backup/" "$ROOT/backup-copy/"
same_file "$ROOT/source/README.txt" "$ROOT/backup-copy/README.txt"
same_file "$ROOT/source/nested dir/main.c" "$ROOT/backup-copy/nested dir/main.c"
same_file "$ROOT/source/café note.txt" "$ROOT/backup-copy/café note.txt"
same_metadata "$ROOT/source/README.txt" "$ROOT/backup-copy/README.txt"
same_metadata "$ROOT/source/nested dir/main.c" "$ROOT/backup-copy/nested dir/main.c"
same_metadata "$ROOT/source/café note.txt" "$ROOT/backup-copy/café note.txt"

echo "== REM loopback restore (desktop -> REM -> readback) =="
"$RSYNC_BIN" --contimeout=5 --timeout=20 -rtp "$ROOT/desktop-input/" "rsync://127.0.0.1:$PORT/restore/"
"$RSYNC_BIN" --contimeout=5 --timeout=20 -rtp "rsync://127.0.0.1:$PORT/restore/" "$ROOT/restore-readback/"
same_file "$ROOT/desktop-input/restore.txt" "$ROOT/restore-readback/restore.txt"
same_file "$ROOT/desktop-input/nested dir/restored.c" "$ROOT/restore-readback/nested dir/restored.c"
same_metadata "$ROOT/desktop-input/restore.txt" "$ROOT/restore-readback/restore.txt"
same_metadata "$ROOT/desktop-input/nested dir/restored.c" "$ROOT/restore-readback/nested dir/restored.c"

echo "PASS: REM rsync version, loopback daemon, recursive backup, restore, contents, timestamps, and file permissions"
echo "Artifacts retained under $ROOT for inspection."
