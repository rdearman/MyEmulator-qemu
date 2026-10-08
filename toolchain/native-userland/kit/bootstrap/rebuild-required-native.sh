#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}/.." ;; esac
source ./native-env.sh
trap 'result=$?; echo "$result" > "$ROOT/COMPILER-REQUIRED-RESULT"' EXIT
seed=${REQUIRED_SEED_CC:-"$ROOT/bootstrap/native/chibicc-update-seed"}
test -x "$seed" || {
    echo "Missing qualified bootstrap compiler: $seed" >&2
    exit 1
}
cp "$seed" bootstrap/native/chibicc-required-seed
compiler="$ROOT/bootstrap/native/chibicc-required-seed"
echo 'REM_STAGE|seed-compiler-smoke'
"$compiler" --help
mkdir -p native-tests/porting
check_small_return() {
    echo "REM_STAGE|aggregate-small-return-$2"
    "$1" "$ROOT/tests/aggregate-small-return.c" -o "$ROOT/native-tests/porting/aggregate-small-return"
    "$ROOT/native-tests/porting/aggregate-small-return"
    echo "PASS aggregate-small-return ($2)"
}
check_small_return "$compiler" seed
cd bootstrap/chibicc
echo 'REM_STAGE|stage1-native-rebuild'
CC="$compiler" bash build-native.sh
cp chibicc-native ../native/chibicc-required-stage1
check_small_return "$ROOT/bootstrap/native/chibicc-required-stage1" stage1
echo 'REM_STAGE|stage2-native-rebuild'
CC="$ROOT/bootstrap/native/chibicc-required-stage1" bash build-native.sh
cp chibicc-native ../native/chibicc-required-stage2
check_small_return "$ROOT/bootstrap/native/chibicc-required-stage2" stage2
cd "$ROOT"
cp bootstrap/chibicc/include/*.h bootstrap/native/include/
echo 'REM_STAGE|compiler-porting-regressions'
TEST_CC="$ROOT/bootstrap/native/chibicc-required-stage2" bash tests/compiler-porting-native.sh
test -f bootstrap/native/chibicc-selfbuilt.before-required ||
    cp bootstrap/native/chibicc-selfbuilt bootstrap/native/chibicc-selfbuilt.before-required
cp bootstrap/native/chibicc-required-stage2 bootstrap/native/chibicc-selfbuilt.new
mv bootstrap/native/chibicc-selfbuilt.new bootstrap/native/chibicc-selfbuilt
echo 'REM_STAGE|compiler-required-complete'
echo COMPILER_REQUIRED_SELFREBUILD_OK
