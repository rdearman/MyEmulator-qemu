#!/bin/sh
set -eu
CC=${CC:-chibicc}
case "${1:-build}" in
  clean) rm -f linegrep; exit 0 ;;
  build|rebuild) ;;
  *) echo 'usage: build.sh [build|rebuild|clean]' >&2; exit 2 ;;
esac
"$CC" -o linegrep linegrep.c
