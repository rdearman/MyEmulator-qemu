#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
CC=${CC:-$ROOT/.toolchain-install/bin/myemulator2-elf-as}
LD=${LD:-$ROOT/.toolchain-install/bin/myemulator2-elf-ld}
CHIBICC=${CHIBICC:-$ROOT/toolchain/chibicc-rem/chibicc}
TMP=${TMPDIR:-/tmp}/chibicc-rem-test-$$
trap 'rm -rf "$TMP"' EXIT
mkdir "$TMP"
cat >"$TMP/test.c" <<'SRC'
int add(int a, int b) { return a + b; }
int main(void) { int i = 0, s = 0; while (i < 5) { s = add(s, i); i = i + 1; } return s; }
SRC
"$CHIBICC" -S -o "$TMP/test.s" "$TMP/test.c"
"$CC" -o "$TMP/test.o" "$TMP/test.s"
"$ROOT/.toolchain-install/bin/myemulator2-elf-readelf" -h "$TMP/test.o" | grep -E 'Class:|Data:|Machine:'
echo "REM chibicc assembly test passed"
