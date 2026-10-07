#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}" ;; esac
INITIAL_CC=${CC:-chibicc}
mkdir -p native
cd chibicc
CC="$INITIAL_CC" REM_BOOTSTRAP_FLOAT=0 BOOTSTRAP_FLAGS=-DREM_INTEGER_BOOTSTRAP \
    bash build-native.sh
cp chibicc-native ../native/chibicc-integer
mkdir -p ../native/include
cp include/* ../native/include/
cd ../compiler-rt-15.0.7.src
CC="$(cd ../native && pwd)/chibicc-integer" bash build-native.sh
export CHIBICC_REM_SOFTFLOAT="$(pwd)/native/ieee-runtime.o"
cd ../chibicc
CC="$(cd ../native && pwd)/chibicc-integer" REM_BOOTSTRAP_FLOAT=1 \
    BOOTSTRAP_FLAGS=-DREM_BOOTSTRAP_FLOAT bash build-native.sh
cp chibicc-native ../native/chibicc-float
cd ../musl
CC="$(cd ../native && pwd)/chibicc-float" bash build-native.sh seed
export CHIBICC_REM_LIBC="$(pwd)/native/libc.a"
cd ../chibicc
CC="$(cd ../native && pwd)/chibicc-float" REM_BOOTSTRAP_FLOAT=0 \
    BOOTSTRAP_FLAGS= bash build-native.sh
cp chibicc-native ../native/chibicc-decimal
cd ../musl
CC="$(cd ../native && pwd)/chibicc-decimal" bash build-native.sh full
cd ../chibicc
CC="$(cd ../native && pwd)/chibicc-decimal" REM_BOOTSTRAP_FLOAT=0 \
    BOOTSTRAP_FLAGS= bash build-native.sh
cp chibicc-native ../native/chibicc
CC="$(cd ../native && pwd)/chibicc" REM_BOOTSTRAP_FLOAT=0 \
    BOOTSTRAP_FLAGS= bash build-native.sh
cp chibicc-native ../native/chibicc-selfbuilt
echo "Compiler rebuilt itself against the private repaired libc; regression tests still required."
