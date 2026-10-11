#!/bin/sh
# Run only with a matched native REM compiler/binutils/runtime tuple.
set -eu
CC=${CC:-chibicc}
AR=${AR:-ar}
RANLIB=${RANLIB:-ranlib}
mkdir -p build bin
"$CC" -c add.c -o build/add.o
"$CC" -c scale.c -o build/scale.o
"$AR" rcs build/libremmath.a build/add.o build/scale.o
"$RANLIB" build/libremmath.a
"$CC" -o bin/library-demo main.c build/libremmath.a
