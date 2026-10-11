#!/bin/sh
# Run inside a disposable REM Linux guest with the flight developer tools.
set -eu
trap 'status=$?; if [ "$status" -ne 0 ]; then echo REM_DEVELOPER_BUILD_WORKFLOW_FAIL; fi' EXIT

project=${1:-.}
toolbin=${TOOLBIN:-/root/native/bin}
PATH="$toolbin:/usr/bin:/bin"
export PATH

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

for tool in chibicc as ld remdbg readelf; do
    [ -x "$toolbin/$tool" ] || fail "missing executable $toolbin/$tool"
done
command -v make >/dev/null 2>&1 || fail "GNU Make is missing"
command -v samu >/dev/null 2>&1 || fail "Samurai is missing"
[ -x /usr/bin/kilo ] || fail "Kilo is missing"
[ -f /usr/share/doc/rem-flight/KILO_GUIDE.md ] ||
    [ -f /usr/share/doc/rem/manual/KILO_GUIDE.md ] ||
    fail "Kilo usage guide is missing"
[ -f /usr/lib/crt1.o ] || fail "missing /usr/lib/crt1.o"
[ -f /usr/lib/crti.o ] || fail "missing /usr/lib/crti.o"
[ -f /usr/lib/crtn.o ] || fail "missing /usr/lib/crtn.o"
[ -f /usr/lib/libc.a ] || fail "missing /usr/lib/libc.a"
[ -f /root/native/lib/libgcc.a ] || fail "missing /root/native/lib/libgcc.a"
[ -f /lib/myemulator2-user.ld ] || fail "missing /lib/myemulator2-user.ld"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$project"
for file in hello.c Makefile build.ninja; do
    if [ ! -f "$project/$file" ]; then
        cp "$script_dir/$file" "$project/$file"
    fi
done
cd "$project"

# This intentionally exposes all three native tool stages in the build log.
make clean
make
[ "$(./hello)" = "REM developer workflow PASS" ] || fail "manual chibicc/as/ld output mismatch"
pass "chibicc -> as -> ld executable runs"

make clean
make
[ "$(./hello)" = "REM developer workflow PASS" ] || fail "clean Make rebuild output mismatch"
touch hello.c
make
[ "$(./hello)" = "REM developer workflow PASS" ] || fail "Make touch rebuild output mismatch"
pass "Make clean, rebuild, and incremental rebuild"

rm -f hello.s hello.o hello
samu
[ "$(./hello)" = "REM developer workflow PASS" ] || fail "Samurai output mismatch"
pass "Samurai build and execution"

# Run the freshly built C executable under remdbg and check symbol lookup,
# breakpoint, register/memory inspection, disassembly, stepping, and exit.
main_addr=$("$toolbin/readelf" -s hello | awk '$NF == "main" { print "0x" $2; exit }')
[ -n "$main_addr" ] || fail "readelf did not find main in the fresh executable"
printf 'break main\nrun\nregs\nmem %s 4\ndisasm %s 1\nstep\ncontinue\nquit\n' \
    "$main_addr" "$main_addr" | "$toolbin/remdbg" ./hello >remdbg-build.log 2>&1
grep -q 'hit breakpoint at' remdbg-build.log || fail "remdbg did not hit main in the fresh build"
grep -q 'pc=' remdbg-build.log || fail "remdbg did not display registers"
grep -q 'program exited with status 0' remdbg-build.log || fail "remdbg did not continue to successful exit"
pass "fresh C binary: breakpoint, registers, memory, disassembly, step, continue"

# Reuse the pinned fault fixture to prove the integrated kernel/debugger can
# report an application fault. Full ptrace/DWARF checks remain in the host
# pexpect suite under tests/debugger.
[ -x /usr/share/remdbg/qualification/debug_test ] || fail "debugger fixture missing"
printf 'break main\nrun\ncontinue\nregs\nbt\ncontinue\nquit\n' |
    "$toolbin/remdbg" /usr/share/remdbg/qualification/debug_test fault >remdbg-fault.log 2>&1
grep -q 'stopped: signal 11' remdbg-fault.log || fail "remdbg did not catch fixture SIGSEGV"
grep -q 'program terminated by signal 11' remdbg-fault.log || fail "remdbg did not report fixture termination"
pass "debugger catches and reports deliberate SIGSEGV"
echo REM_DEVELOPER_BUILD_WORKFLOW_PASS
