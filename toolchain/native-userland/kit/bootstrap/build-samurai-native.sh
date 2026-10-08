#!/bin/bash
# Build Samurai with the qualified native chibicc and install it as
# bootstrap/tools/native/samu (on PATH via native-env.sh), then exercise it.
set -eu
case $0 in */*) cd "${0%/*}/.." ;; esac
source ./native-env.sh
src="$ROOT/packages/samurai"
test -f "$src/samu.c" || { echo "Samurai source missing: $src" >&2; exit 1; }
cd "$src"
objects=
for name in build deps env graph htab log os-posix parse samu scan tool tree util; do
    echo "CC samurai $name.c"
    "$CC" -DNO_POSIX_SPAWN -DCHIBICC_REM -I. -c -o "$name.o" "$name.c"
    objects="$objects $name.o"
done
ld -r -T "$ROOT/bootstrap/rem-reloc.ld" -o samu-partial.o $objects
"$CC" -o samu samu-partial.o
mkdir -p "$ROOT/bootstrap/tools/native"
cp samu "$ROOT/bootstrap/tools/native/samu.new"
mv "$ROOT/bootstrap/tools/native/samu.new" "$ROOT/bootstrap/tools/native/samu"
echo SAMURAI_BUILD_OK
samu="$ROOT/bootstrap/tools/native/samu"
run="$src/rem-selftest"
rm -rf "$run"
mkdir -p "$run"
cd "$run"
printf 'int main(void) { return 42; }\n' > hello.c
cat > build.ninja <<NINJA
rule cc
  command = $CC -o \$out \$in
build hello: cc hello.c
default hello
NINJA
"$samu" -n
test ! -e hello
echo SAMURAI_DRY_RUN_OK
"$samu"
if ./hello; then rc=0; else rc=$?; fi
test "$rc" -eq 42
echo SAMURAI_NATIVE_BUILD_OK
sleep 1
printf 'int main(void) { return 43; }\n' > hello.c
"$samu"
if ./hello; then rc=0; else rc=$?; fi
test "$rc" -eq 43
echo SAMURAI_INCREMENTAL_REBUILD_OK
"$samu" -t clean
test ! -e hello
echo SAMURAI_CLEAN_OK
"$samu"
if ./hello; then rc=0; else rc=$?; fi
test "$rc" -eq 43
echo SAMURAI_NATIVE_EXECUTION_OK
