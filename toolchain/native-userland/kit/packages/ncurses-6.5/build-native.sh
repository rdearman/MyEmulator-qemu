#!/bin/bash
set -Eeuo pipefail
ROOT=${REM_USERLAND_ROOT:?REM_USERLAND_ROOT must be exported}
export REM_USERLAND_ROOT
PACKAGE_ROOT=$ROOT/packages/ncurses-6.5
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
mkdir -p "$ROOT/native-deps/include" "$ROOT/native-deps/lib" "$ROOT/native-deps/share/terminfo"
for header in native-build/include/*.h; do atomic_copy "$header" "$ROOT/native-deps/include/${header##*/}"; done
atomic_copy native-build/lib/libncurses.a "$ROOT/native-deps/lib/libncurses.a"
native/bin/tic -x -e ansi,vt100,xterm,xterm-256color,linux -o "$ROOT/native-deps/share/terminfo" misc/terminfo.src
mkdir -p "$ROOT/native-tests"
"$CC" -I"$ROOT/native-deps/include" "$ROOT/tests/vim-terminfo-contracts.c" -L"$ROOT/native-deps/lib" -lncurses -o "$ROOT/native-tests/vim-terminfo-contracts.tmp"
mv -f "$ROOT/native-tests/vim-terminfo-contracts.tmp" "$ROOT/native-tests/vim-terminfo-contracts"
TERMINFO="$ROOT/native-deps/share/terminfo" "$ROOT/native-tests/vim-terminfo-contracts"
