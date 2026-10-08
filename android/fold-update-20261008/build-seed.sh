#!/bin/bash
# Cross-build the explicit native bootstrap compiler. Qualification runs in REM.
set -euo pipefail
here=$(cd "${0%/*}" && pwd)
repo=$(cd "$here/../.." && pwd)
out=${1:-/tmp/rem-fold-update-seed}
case "$out" in /tmp/*) ;; *) echo "seed build must use a disposable /tmp directory" >&2; exit 1 ;; esac
mkdir -p "$out"
out=$(cd "$out" && pwd)
cd "$repo"
export MYEMU_MUSL_PREFIX="$repo/toolchain/flight-kit-runtime"
export MYEMU_TARGET_GCC="$repo/.toolchain-install/bin/myemulator2-elf-gcc"
objects=()
for name in codegen hashmap main parse preprocess rem_float strings tokenize type unicode; do
    toolchain/scripts/myemulator2-musl-gcc -std=c11 -O0 -DCHIBICC_REM \
        -Itoolchain/flight-kit-runtime/include -Itoolchain/chibicc-rem \
        -c "toolchain/chibicc-rem/$name.c" -o "$out/$name.o"
    objects+=("$out/$name.o")
done
toolchain/scripts/myemulator2-musl-gcc -nostdlib "${objects[@]}" \
    -Ltoolchain/flight-kit-runtime/lib -Wl,--start-group -lc -lgcc -Wl,--end-group \
    -o "$out/chibicc-update-seed"
sha256sum "$out/chibicc-update-seed"
