#!/bin/sh
# Make-independent fallback. Run in this directory with the matching REM
# compiler driver configured for the installed native tools and sysroot.
set -eu
CC=${CC:-chibicc}
AS=${AS:-as}
LD=${LD:-ld}
OUT=${OUT:-hello}
case "${1:-build}" in
  clean) rm -f hello.s hello.o "$OUT"; exit 0 ;;
  build|rebuild) ;;
  *) echo "usage: $0 [build|rebuild|clean]" >&2; exit 2 ;;
esac
if [ "${1:-build}" = rebuild ]; then rm -f hello.s hello.o "$OUT"; fi
"$CC" -S -o hello.s hello.c
"$AS" -o hello.o hello.s
"$LD" -T "${LINKER_SCRIPT:-/usr/lib/myemulator2-user.ld}" \
  -o "$OUT" "${CRTDIR:-/usr/lib}/crt1.o" "${CRTDIR:-/usr/lib}/crti.o" \
  hello.o -L"${LIBDIR:-/usr/lib}" --start-group -lc -lgcc --end-group \
  "${CRTDIR:-/usr/lib}/crtn.o"
