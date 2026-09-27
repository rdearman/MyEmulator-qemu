#!/bin/sh
set -eu
CC=${CC:-chibicc}
case "${1:-build}" in
  clean) rm -f main.o hexscope.o elf32.o hexscope; exit 0 ;;
  build|rebuild) ;;
  *) echo 'usage: build.sh [build|rebuild|clean]' >&2; exit 2 ;;
esac
"$CC" -c -o main.o main.c
"$CC" -c -o hexscope.o hexscope.c
"$CC" -c -o elf32.o elf32.c
"$CC" -o hexscope main.o hexscope.o elf32.o
