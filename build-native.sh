#!/bin/sh
# Build chibicc natively on REM with the installed chibicc, as and ld.  No make.
# Output: ./chibicc-native  (the installed compiler is NOT touched).
# "ld -r" first, then the final link: a one-step native link of all objects
# crashed ld historically, so this is the established workaround.
set -eu
CC=${CC:-chibicc}
OBJS=""
for f in codegen hashmap main parse preprocess strings tokenize type unicode rem_float; do
	echo "CC $f.c"
	"$CC" -DCHIBICC_REM ${BOOTSTRAP_FLAGS:-} -I. -c -o "$f.o" "$f.c"
	OBJS="$OBJS $f.o"
done
if [ "${REM_BOOTSTRAP_FLOAT:-0}" = 1 ]; then
    "$CC" -DRYU_ONLY_64_BIT_OPS -I../ryu -c ../ryu/ryu/s2d.c -o ryu-s2d.o
    OBJS="$OBJS ryu-s2d.o"
fi
ld -r -T ../rem-reloc.ld -o chibicc-partial.o $OBJS
"$CC" -o chibicc-native chibicc-partial.o
ls -l chibicc-native
echo "To try it:   ./chibicc-native -g hello.c -o hello   (uses ./include next to the binary)"
echo "To install:  cp ~/.local/bin/chibicc ~/.local/bin/chibicc.orig; cp chibicc-native ~/.local/bin/chibicc"
