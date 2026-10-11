#!/bin/bash
set -Eeuo pipefail
ROOT=${REM_USERLAND_ROOT:?REM_USERLAND_ROOT must be exported}
export REM_USERLAND_ROOT
PACKAGE_ROOT=$ROOT/packages/diffutils-3.10
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
