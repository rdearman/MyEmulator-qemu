#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}/.." ;; esac
source ./native-env.sh
trap 'result=$?; echo "$result" > "$ROOT/COMPILER-REQUIRED-RESULT"' EXIT
cp "$CC" bootstrap/native/chibicc-required-seed
compiler="$ROOT/bootstrap/native/chibicc-required-seed"
"$compiler" --help
cd bootstrap/chibicc
CC="$compiler" bash build-native.sh
cp chibicc-native ../native/chibicc-selfbuilt.new
mv ../native/chibicc-selfbuilt.new ../native/chibicc-selfbuilt
CC="$ROOT/bootstrap/native/chibicc-selfbuilt" bash build-native.sh
cp chibicc-native ../native/chibicc-selfbuilt.new
mv ../native/chibicc-selfbuilt.new ../native/chibicc-selfbuilt
cd "$ROOT"
cp bootstrap/chibicc/include/*.h bootstrap/native/include/
bash tests/compiler-porting-native.sh
echo COMPILER_REQUIRED_SELFREBUILD_OK
