#!/bin/sh
# Runs on the pinned REM guest. Host setup/restart is in the Python runner.
set +e
BIN=/root/binqual/bin
LIB=/root/binqual/lib
WORK=/root/binqual/work
AS=$BIN/as
# The qualified self-hosted linker is the newer native artifact installed in
# /root/native/bin. The legacy /usr path has a distinct hash and crashes on
# -r; do not silently substitute it for this qualification.
LD=/root/native/bin/ld
AR=$BIN/ar
RANLIB=$BIN/ranlib
READELF=$BIN/readelf
OBJDUMP=$BIN/objdump
FAILURES=0
pass() { echo "PASS $1"; }
fail() { echo "FAIL $1"; FAILURES=1; }

chmod 0755 "$BIN"/* 2>/dev/null
cd "$WORK" || exit 90

echo '--- native versions ---'
for tool in as ld ar ranlib readelf objdump; do
	"$BIN/$tool" --version
done

"$AS" -o ld-empty.o /root/binqual/src/empty.S >ld-empty-as.log 2>&1
empty_as_rc=$?
"$AS" -o ld-hello.o /root/binqual/src/ld-hello.S >ld-hello-as.log 2>&1
ld_hello_as_rc=$?
if [ "$empty_as_rc" -eq 0 ] && [ "$ld_hello_as_rc" -eq 0 ]; then pass 'as minimal and hello linker inputs'; else fail 'as minimal linker inputs'; fi
"$LD" -r ld-empty.o -o ld-empty-rel.o >ld-empty-rel.log 2>&1
if [ $? -eq 0 ]; then pass 'native root linker -r empty object'; else fail 'native root linker -r empty object'; fi
"$LD" -r ld-hello.o -o ld-hello-rel.o >ld-hello-rel.log 2>&1
if [ $? -eq 0 ]; then pass 'native root linker -r hello object'; else fail 'native root linker -r hello object'; fi
"$READELF" -h -r ld-hello-rel.o >ld-hello-rel.header 2>&1
if [ $? -eq 0 ] && [ -s ld-hello-rel.header ]; then pass 'readelf inspects native ld relocatable output'; else fail 'readelf native ld relocatable output'; fi
"$LD" -T /root/native/lib/myemulator2-user.ld -o ld-hello ld-hello.o >ld-hello-link.log 2>&1
if [ $? -eq 0 ]; then
	ld_hello_output=$(./ld-hello 2>&1); ld_hello_rc=$?
	if [ "$ld_hello_output" = 'REM NATIVE LINK PASS' ] && [ "$ld_hello_rc" -eq 0 ]; then pass 'native root linker hello executable'; else fail 'native root linker hello execution'; fi
else
	fail 'native root linker hello link'
fi
OLD_LD=/usr/myemulator2-linux-musl/bin/ld
"$OLD_LD" -r ld-hello.o -o legacy-path-ld-hello-rel.o >legacy-path-ld.log 2>&1
legacy_ld_rc=$?
echo "LEGACY_PATH_LD_R_RC:$legacy_ld_rc"
if [ "$legacy_ld_rc" -eq 139 ]; then echo 'OBSERVED legacy PATH ld crash on -r'; fi

"$AS" -o single.o /root/binqual/src/single.S >single-as.log 2>&1
if [ $? -eq 0 ]; then pass 'as assembles single object'; else fail 'as single object'; fi
"$READELF" -h single.o >single.header 2>&1
if [ $? -eq 0 ] && [ -s single.header ]; then pass 'readelf object header'; else fail 'readelf object header'; fi
"$READELF" -S -s -r single.o >single.sections 2>&1
if [ $? -eq 0 ] && [ -s single.sections ]; then pass 'readelf object sections/symbols/relocations'; else fail 'readelf object sections/symbols/relocations'; fi
"$OBJDUMP" -f -h -t -d single.o >single.dis 2>&1
if [ $? -eq 0 ] && [ -s single.dis ]; then pass 'objdump object headers/sections/symbols/disassembly'; else fail 'objdump object'; fi
"$RANLIB" host-index-input.a >host-ranlib.log 2>&1
host_ranlib_rc=$?
if [ "$host_ranlib_rc" -eq 0 ]; then pass 'ranlib indexes controlled REM archive input'; else fail 'ranlib indexes controlled REM archive input'; fi
"$READELF" -h -s host-index-input.a >host-archive.readelf 2>&1
host_archive_readelf_rc=$?
if [ "$host_archive_readelf_rc" -eq 0 ] && [ -s host-archive.readelf ]; then pass 'readelf inspects archive members'; else fail 'readelf archive members'; fi
"$LD" -T /root/native/lib/myemulator2-user.ld -o single single.o >single-ld.log 2>&1
if [ $? -eq 0 ]; then pass 'ld links single object'; else fail 'ld single object'; fi
single_output=$(./single 2>&1); single_rc=$?
if [ "$single_output" = 'SINGLE_OK' ] && [ "$single_rc" -eq 0 ]; then pass 'single executable output/exit'; else fail 'single executable output/exit'; fi
"$READELF" -h -S -s -l single >single.exe.readelf 2>&1
if [ $? -eq 0 ] && [ -s single.exe.readelf ]; then pass 'readelf executable/program headers'; else fail 'readelf executable'; fi
"$OBJDUMP" -d single >single.exe.dis 2>&1
if [ $? -eq 0 ] && [ -s single.exe.dis ]; then pass 'objdump executable disassembly'; else fail 'objdump executable'; fi

"$AS" -o multi.o /root/binqual/src/multi.S >multi-as.log 2>&1
multi_as_rc=$?
"$AS" -o helper.o /root/binqual/src/helper.S >helper-as.log 2>&1
helper_as_rc=$?
if [ "$multi_as_rc" -eq 0 ] && [ "$helper_as_rc" -eq 0 ]; then pass 'as multiple source objects'; else fail 'as multi objects'; fi
"$READELF" -r multi.o >multi.relocs 2>&1
if [ $? -eq 0 ] && [ -s multi.relocs ]; then pass 'readelf external relocation'; else fail 'readelf relocation'; fi
"$LD" -T /root/native/lib/myemulator2-user.ld -o multi multi.o helper.o >multi-ld.log 2>&1
if [ $? -eq 0 ]; then pass 'ld resolves cross-object symbol'; else fail 'ld multi objects'; fi
multi_output=$(./multi 2>&1); multi_rc=$?
if [ "$multi_output" = 'MULTI_OK' ] && [ "$multi_rc" -eq 0 ]; then pass 'multi-object executable output/exit'; else fail 'multi-object executable run'; fi

"$AS" -o archive-main.o /root/binqual/src/archive-main.S >archive-main-as.log 2>&1
main_as_rc=$?
"$AS" -o archive-a.o /root/binqual/src/archive-a.S >archive-a-as.log 2>&1
a_as_rc=$?
"$AS" -o archive-b.o /root/binqual/src/archive-b-v1.S >archive-b-as.log 2>&1
b_as_rc=$?
if [ "$main_as_rc" -eq 0 ] && [ "$a_as_rc" -eq 0 ] && [ "$b_as_rc" -eq 0 ]; then pass 'as archive fixture objects'; else fail 'as archive fixture objects'; fi
"$AR" rcs libexample.a archive-a.o archive-b.o >ar-create.log 2>&1
ar_rc=$?
ranlib_rc=125
list_rc=125
extract_rc=125
member_rc=125
archive_link_rc=125
if [ "$ar_rc" -eq 0 ]; then
	"$RANLIB" libexample.a >ranlib-create.log 2>&1
	ranlib_rc=$?
	"$AR" t libexample.a >archive.list 2>&1
	list_rc=$?
	(cd extracted && "$AR" x ../libexample.a archive-a.o) >ar-extract.log 2>&1
	extract_rc=$?
	if [ "$extract_rc" -eq 0 ] && [ -s extracted/archive-a.o ]; then
		"$READELF" -h -s extracted/archive-a.o >archive.member.readelf 2>&1
		member_rc=$?
	fi
	"$LD" -T /root/native/lib/myemulator2-user.ld -o archive-app archive-main.o --start-group libexample.a --end-group >archive-link.log 2>&1
	archive_link_rc=$?
fi
if [ "$ar_rc" -eq 0 ] && [ "$ranlib_rc" -eq 0 ] && [ "$list_rc" -eq 0 ] && [ -s archive.list ]; then pass 'ar create/list and ranlib index'; else fail 'ar create/list and ranlib index'; fi
if [ "$extract_rc" -eq 0 ] && [ "$member_rc" -eq 0 ]; then pass 'ar extracts member and readelf inspects it'; else fail 'ar extraction/readelf member'; fi
if [ "$archive_link_rc" -eq 0 ]; then pass 'ld links executable against created archive'; else fail 'ld archive link'; fi
if [ "$archive_link_rc" -eq 0 ]; then
	archive_out=$(./archive-app 2>&1); archive_rc=$?
	if [ "$archive_out" = 'ARCHIVE_42' ] && [ "$archive_rc" -eq 0 ]; then pass 'archive executable output/exit'; else fail 'archive executable run'; fi
fi

# Replace just one object, update/index the same archive, relink and check the
# marker changes from 42 to 43. No make or Samurai is involved.
if [ "$ar_rc" -eq 0 ] && [ "$archive_link_rc" -eq 0 ]; then
	"$AS" -o archive-b.o /root/binqual/src/archive-b-v2.S >archive-b-update.log 2>&1
	as_update_rc=$?
	"$AR" r libexample.a archive-b.o >ar-update.log 2>&1
	ar_update_rc=$?
	"$RANLIB" libexample.a >ranlib-update.log 2>&1
	ranlib_update_rc=$?
	"$LD" -T /root/native/lib/myemulator2-user.ld -o archive-app archive-main.o --start-group libexample.a --end-group >archive-relink.log 2>&1
	relink_rc=$?
	archive_out=$(./archive-app 2>&1); archive_rc=$?
	if [ "$as_update_rc" -eq 0 ] && [ "$ar_update_rc" -eq 0 ] && [ "$ranlib_update_rc" -eq 0 ] && [ "$relink_rc" -eq 0 ] && [ "$archive_out" = 'ARCHIVE_43' ] && [ "$archive_rc" -eq 0 ]; then pass 'manual archive update/rebuild changes executable'; else fail 'manual archive update/rebuild'; fi
fi

# Use validated chibicc only as an assembly producer; native as/ld do the
# compilation boundary and link. The regular musl/libgcc hosted path is tested.
"$BIN/chibicc" -S -o hello.s /root/binqual/src/hello.c >hello-cc.log 2>&1
cc_rc=$?
if [ "$cc_rc" -eq 0 ]; then
	"$AS" -o hello.o hello.s >hello-as.log 2>&1
	cc_as_rc=$?
	"$READELF" -h hello.o >hello.header 2>&1
	readelf_hello_rc=$?
	if [ "$cc_as_rc" -eq 0 ] && [ "$readelf_hello_rc" -eq 0 ] && [ -s hello.header ]; then pass 'C -> chibicc assembly -> native as'; else fail 'C/native as pipeline'; fi
	"$LD" -T /root/native/lib/myemulator2-user.ld -o c-hello \
		-L/usr/lib -L"$LIB" /usr/lib/crt1.o /usr/lib/crti.o hello.o \
		--start-group -lc -lgcc --end-group /usr/lib/crtn.o >hello-link.log 2>&1
	c_link_rc=$?
	if [ "$c_link_rc" -eq 0 ]; then
		c_out=$(./c-hello 2>&1); c_rc=$?
		if [ "$c_out" = 'CHIBICC_BINUTILS_OK' ] && [ "$c_rc" -eq 0 ]; then pass 'native ld libc/libgcc C executable'; else fail 'C executable output/exit'; fi
	else
		fail 'native ld libc/libgcc C link'
	fi
else
	fail 'chibicc emitted C assembly (compiler-owned boundary)'
fi

# C and assembly objects in a native archive; assembly calls a function from
# chibicc-generated assembly, and an external REM _start calls the archive.
if [ "$cc_rc" -ne 0 ]; then
	echo 'BLOCKED C/assembly archive fixture after chibicc rejected the C fixture'
else
"$BIN/chibicc" -S -o archive-c-value.s /root/binqual/src/archive-c-value.c >archive-c-cc.log 2>&1
c_archive_cc_rc=$?
if [ "$c_archive_cc_rc" -eq 0 ]; then
	"$AS" -o archive-c-value.o archive-c-value.s >archive-c-as.log 2>&1
	value_as_rc=$?
	"$AS" -o archive-c-adapter.o /root/binqual/src/archive-c-adapter.S >>archive-c-as.log 2>&1
	adapter_as_rc=$?
	"$AS" -o archive-c-main.o /root/binqual/src/archive-c-main.S >>archive-c-as.log 2>&1
	main_as_rc=$?
	"$AR" rcs libc-assembly.a archive-c-value.o archive-c-adapter.o >archive-c-ar.log 2>&1
	c_ar_rc=$?
	"$RANLIB" libc-assembly.a >>archive-c-ar.log 2>&1
	c_ranlib_rc=$?
	"$LD" -T /root/native/lib/myemulator2-user.ld -o c-assembly-app archive-c-main.o --start-group libc-assembly.a --end-group >archive-c-ld.log 2>&1
	c_ld_rc=$?
	c_archive_out=$(./c-assembly-app 2>&1); c_archive_run_rc=$?
	if [ "$value_as_rc" -eq 0 ] && [ "$adapter_as_rc" -eq 0 ] && [ "$main_as_rc" -eq 0 ] && [ "$c_ar_rc" -eq 0 ] && [ "$c_ranlib_rc" -eq 0 ] && [ "$c_ld_rc" -eq 0 ] && [ "$c_archive_run_rc" -eq 0 ] && [ "$c_archive_out" = 'C_ARCHIVE_OK' ]; then pass 'C+assembly -> archive -> native ld -> execute'; else fail 'C/assembly archive'; fi
else
	fail 'chibicc emitted C/assembly archive source (compiler-owned boundary)'
fi
fi

# Bounded error cases: nonzero exit, no signal/segfault, and diagnostics saved.
"$AS" -o unresolved.o /root/binqual/src/unresolved.S >unresolved-as.log 2>&1
"$LD" -T /root/native/lib/myemulator2-user.ld -o unresolved unresolved.o >unresolved.log 2>&1
undefined_rc=$?
if [ "$undefined_rc" -ne 0 ] && [ "$undefined_rc" -ne 139 ] && [ -s unresolved.log ]; then pass 'ld unresolved-symbol diagnostic'; else fail 'ld unresolved symbol/no crash'; fi
"$AS" -o duplicate-a.o /root/binqual/src/duplicate-a.S >duplicate-a.log 2>&1
"$AS" -o duplicate-b.o /root/binqual/src/duplicate-b.S >duplicate-b.log 2>&1
"$LD" -T /root/native/lib/myemulator2-user.ld -o duplicate duplicate-a.o duplicate-b.o >duplicate.log 2>&1
duplicate_rc=$?
if [ "$duplicate_rc" -ne 0 ] && [ "$duplicate_rc" -ne 139 ] && [ -s duplicate.log ]; then pass 'ld duplicate-symbol diagnostic'; else fail 'ld duplicate symbol/no crash'; fi
"$AS" -o invalid.o /root/binqual/src/invalid.S >invalid.log 2>&1
invalid_rc=$?
if [ "$invalid_rc" -ne 0 ] && [ "$invalid_rc" -ne 139 ] && [ -s invalid.log ]; then pass 'as invalid-instruction diagnostic'; else fail 'as invalid input/no crash'; fi
"$LD" --definitely-not-a-valid-option >bad-ld.log 2>&1
bad_ld_rc=$?
if [ "$bad_ld_rc" -ne 0 ] && [ "$bad_ld_rc" -ne 139 ] && [ -s bad-ld.log ]; then pass 'ld malformed-option diagnostic'; else fail 'ld malformed input/no crash'; fi
"$AR" t absent-archive.a >missing-ar.log 2>&1
missing_ar_rc=$?
if [ "$missing_ar_rc" -ne 0 ] && [ "$missing_ar_rc" -ne 139 ] && [ -s missing-ar.log ]; then pass 'ar missing archive diagnostic'; else fail 'ar missing input/no crash'; fi

echo "BINUTILS_GUEST_FAILURES:$FAILURES"
sync
exit "$FAILURES"
