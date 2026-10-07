#!/bin/bash
ROOT=$(cd "${BASH_SOURCE[0]%/*}" && pwd)
export CC="$ROOT/bootstrap/native/chibicc-selfbuilt"
export CHIBICC_REM_LIBC="$ROOT/bootstrap/musl/native/libc.a"
export CHIBICC_REM_SOFTFLOAT="$ROOT/bootstrap/compiler-rt-15.0.7.src/native/ieee-runtime.o"
export REM_PREFIX=${REM_PREFIX:-"$HOME/.local"}
export CONFIG_SHELL=/bin/bash
export LC_ALL=C
export PATH="$ROOT/packages/coreutils-9.7/native/bin:$ROOT/bootstrap/make-4.4.1/native/bin:$ROOT/bootstrap/tools/native:$PATH"
test -x "$CC" && test -f "$CHIBICC_REM_LIBC" &&
    test -f "$CHIBICC_REM_SOFTFLOAT" || {
    echo "Repaired native bootstrap is missing; run build-all-native.sh." >&2
    return 1
}
