#!/bin/sh
set -eu
CC=${CC:-chibicc}
AS=${AS:-as}
case "${1:-rem}" in
  clean) rm -f main.o bitcount_rem.o bitcount-rem bitcount-portable; exit 0 ;;
  rem)
    "$CC" -c -o main.o main.c
    "$AS" -o bitcount_rem.o bitcount_rem.S
    "$CC" -o bitcount-rem main.o bitcount_rem.o
    ;;
  portable)
    HOSTCC=${HOSTCC:-cc}
    "$HOSTCC" -o bitcount-portable main.c bitcount_portable.c
    ;;
  *) echo 'usage: build.sh [rem|portable|clean]' >&2; exit 2 ;;
esac
