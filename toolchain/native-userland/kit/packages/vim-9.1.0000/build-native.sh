#!/bin/bash
set -Eeuo pipefail
ROOT=${REM_USERLAND_ROOT:?REM_USERLAND_ROOT must be exported}
export REM_USERLAND_ROOT
PACKAGE_ROOT=$ROOT/packages/vim-9.1.0000
source "$ROOT/native-env.sh"
cd "$PACKAGE_ROOT"
atomic_copy() {
    source=$1 target=$2
    mkdir -p "${target%/*}"
    cp "$source" "$target.tmp"
    mv -f "$target.tmp" "$target"
}
atomic_link() {
    source=$1 name=$2
    rm -f "native/bin/$name.tmp"
    ln -s "$source" "native/bin/$name.tmp"
    mv -f "native/bin/$name.tmp" "native/bin/$name"
}
bash "$ROOT/bootstrap/build-plan-native.sh" "$PACKAGE_ROOT"
mkdir -p "$PACKAGE_ROOT/native/bin" "$ROOT/native-tests"
[[ -s $ROOT/native-deps/lib/libncurses.a ]] || { echo "ncurses library missing" >&2; exit 1; }
"$CC" "$ROOT/tests/vim-libc-contracts.c" -o "$ROOT/native-tests/vim-libc-contracts.tmp"
mv -f "$ROOT/native-tests/vim-libc-contracts.tmp" "$ROOT/native-tests/vim-libc-contracts"
(cd "$ROOT" && native-tests/vim-libc-contracts)
native/bin/vim --version
