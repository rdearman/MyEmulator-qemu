#!/bin/sh
set +e
B=/root/binqual/bin
W=/root/binqual/work
F=/root/binqual/src
L=/root/native/bin/ld
S=/root/native/lib/myemulator2-user.ld
FAIL=0
ok(){ echo "PASS $1"; }
bad(){ echo "FAIL $1"; FAIL=1; }
cd "$W" || exit 90
"$B/as" -o ld-hello.o "$F/ld-hello.S" && "$L" -r ld-hello.o -o ld-hello-rel.o && "$B/readelf" -h -r ld-hello-rel.o >ld-hello-rel.header && ok 'as, native ld -r, readelf' || bad 'as/native ld -r/readelf'
"$L" -T "$S" -o ld-hello ld-hello.o && [ "$(./ld-hello)" = 'REM NATIVE LINK PASS' ] && ok 'native ld executable link/run' || bad 'native ld executable'
"$B/as" -o single.o "$F/single.S" && "$B/readelf" -h single.o >single.header && "$B/readelf" -S -s -r single.o >single.sections && "$B/objdump" -f -h -t -d single.o >single.dis && "$L" -T "$S" -o single single.o && [ "$(./single)" = 'SINGLE_OK' ] && "$B/readelf" -h -S -s -l single >single.exe.readelf && "$B/objdump" -d single >single.exe.dis && ok 'single object, execute, readelf, objdump' || bad 'single object'
"$B/as" -o multi.o "$F/multi.S" && "$B/as" -o helper.o "$F/helper.S" && "$L" -T "$S" -o multi multi.o helper.o && [ "$(./multi)" = 'MULTI_OK' ] && "$B/readelf" -r multi.o >multi.relocs && ok 'multiple objects, execute, relocation inspect' || bad 'multiple objects'
"$B/as" -o archive-main.o "$F/archive-main.S" && "$B/as" -o archive-a.o "$F/archive-a.S" && "$B/as" -o archive-b.o "$F/archive-b-v1.S" || bad 'archive source assembly'
"$B/ar" rcs libexample.a archive-a.o archive-b.o && "$B/ranlib" libexample.a && "$B/ar" t libexample.a >archive.list && "$B/ar" x libexample.a archive-a.o && "$B/readelf" -h -s archive-a.o >archive.member.readelf && ok 'ar create/list/extract, ranlib index, readelf member' || bad 'archive create/index/extract'
"$L" -T "$S" -o archive-app archive-main.o --start-group libexample.a --end-group && [ "$(./archive-app)" = 'ARCHIVE_42' ] && ok 'link/run against archive' || bad 'link against archive'
"$B/as" -o archive-b.o "$F/archive-b-v2.S" && "$B/ar" r libexample.a archive-b.o && "$B/ranlib" libexample.a && "$L" -T "$S" -o archive-app archive-main.o --start-group libexample.a --end-group && [ "$(./archive-app)" = 'ARCHIVE_43' ] && ok 'archive object update/reindex/relink changes output' || bad 'archive update/rebuild'
"$L" -T "$S" -o unresolved unresolved.o >unresolved.log 2>&1; rc=$?; [ "$rc" -ne 0 ] && [ -s unresolved.log ] && ok 'undefined symbol diagnostic' || bad 'undefined symbol error'
echo "BINUTILS_GUEST_FAILURES:$FAIL"
sync
exit "$FAIL"
