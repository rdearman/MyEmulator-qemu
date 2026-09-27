#!/bin/sh
set -eu
CC=${CC:-chibicc}
AR=${AR:-ar}
case "${1:-build}" in
  clean) rm -f main.o note_store.o libnote.a memo; exit 0 ;;
  build|rebuild) ;;
  *) echo 'usage: build.sh [build|rebuild|clean]' >&2; exit 2 ;;
esac
"$CC" -c -o note_store.o note_store.c
"$AR" rcs libnote.a note_store.o
"$CC" -c -o main.o main.c
"$CC" -o memo main.o libnote.a
