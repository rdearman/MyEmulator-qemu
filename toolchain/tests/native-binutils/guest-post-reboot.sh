#!/bin/bash
set +e
BIN=/root/binqual/bin
WORK=/root/binqual/work
FAILURES=0
pass() { echo "PASS $1"; }
fail() { echo "FAIL $1"; FAILURES=1; }
cd "$WORK" || exit 90

"$BIN/readelf" -h -S -l multi >post.archive.header 2>&1
readelf_rc=$?
if [ "$readelf_rc" -eq 0 ] && [ -s post.archive.header ]; then pass 'readelf post-reboot executable captured'; else fail 'readelf post-reboot executable'; fi
"$BIN/objdump" -d archive-app >post.archive.dis 2>&1
objdump_rc=$?
if [ "$objdump_rc" -eq 0 ] && [ -s post.archive.dis ]; then pass 'objdump post-reboot disassembly captured'; else fail 'objdump post-reboot executable'; fi
out=$(./single 2>&1); rc=$?
if [ "$out" = 'SINGLE_OK' ] && [ "$rc" -eq 0 ]; then pass 'single executable survives reboot'; else fail 'single executable post-reboot run'; echo "output=$out rc=$rc"; fi
out=$(./multi 2>&1); rc=$?
if [ "$out" = 'MULTI_OK' ] && [ "$rc" -eq 0 ]; then pass 'multi-object executable survives reboot'; else fail 'multi-object executable post-reboot run'; echo "output=$out rc=$rc"; fi
out=$(./archive-app 2>&1); rc=$?
if [ "$out" = 'ARCHIVE_43' ] && [ "$rc" -eq 0 ]; then pass 'updated archive-linked executable survives reboot'; else fail 'updated archive executable post-reboot run'; echo "output=$out rc=$rc"; fi
if [ -x ./c-hello ]; then
	out=$(./c-hello 2>&1); rc=$?
	if [ "$out" = 'CHIBICC_BINUTILS_OK' ] && [ "$rc" -eq 0 ]; then pass 'C/libc executable survives reboot'; else fail 'C executable post-reboot run'; echo "output=$out rc=$rc"; fi
else
	echo 'BLOCKED C/libc executable post-reboot run (compiler fixture did not produce it)'
fi
echo "BINUTILS_POST_REBOOT_FAILURES:$FAILURES"
sync
exit "$FAILURES"
