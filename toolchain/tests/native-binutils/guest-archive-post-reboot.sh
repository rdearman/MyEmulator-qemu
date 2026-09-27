#!/bin/sh
set +e
B=/root/binqual/bin
W=/root/binqual/work
FAIL=0
cd "$W" || exit 90
"$B/readelf" -h archive-app >post.archive.header 2>&1 && [ -s post.archive.header ] && echo 'PASS readelf persisted executable' || { echo 'FAIL readelf persisted executable'; FAIL=1; }
"$B/objdump" -d archive-app >post.archive.dis 2>&1 && [ -s post.archive.dis ] && echo 'PASS objdump persisted executable' || { echo 'FAIL objdump persisted executable'; FAIL=1; }
out=$(./archive-app 2>&1); rc=$?; [ "$out" = 'ARCHIVE_43' ] && [ "$rc" -eq 0 ] && echo 'PASS updated archive-linked program survives reboot' || { echo "FAIL persisted archive app: $out ($rc)"; FAIL=1; }
out=$(./single 2>&1); rc=$?; [ "$out" = 'SINGLE_OK' ] && [ "$rc" -eq 0 ] && echo 'PASS single executable survives reboot' || { echo "FAIL persisted single: $out ($rc)"; FAIL=1; }
out=$(./multi 2>&1); rc=$?; [ "$out" = 'MULTI_OK' ] && [ "$rc" -eq 0 ] && echo 'PASS multi executable survives reboot' || { echo "FAIL persisted multi: $out ($rc)"; FAIL=1; }
echo "BINUTILS_POST_REBOOT_FAILURES:$FAIL"
sync
exit "$FAIL"
