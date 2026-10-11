#!/bin/bash
set -Eeuo pipefail
USERLAND_ROOT=${REM_USERLAND_ROOT:?REM_USERLAND_ROOT must be exported}
PKG_ROOT=$USERLAND_ROOT/packages/bzip2-1.0.8
SAMU=${SAMU:-samu}
PATH=$USERLAND_ROOT/bootstrap/tools/native:$USERLAND_ROOT/packages/coreutils-9.7/native/bin:/usr/bin:/bin:$PATH
export PATH
command -v "$SAMU" >/dev/null 2>&1 || { echo 'Samurai (samu) is missing' >&2; exit 1; }
[[ -s $PKG_ROOT/build.ninja ]] || { echo "bzip2 Ninja file missing: $PKG_ROOT/build.ninja" >&2; exit 1; }
cd "$PKG_ROOT"
echo 'REM_STAGE|bzip2-samu|start'
"$SAMU" -j1
for output in native/libbz2.a native/bin/bzip2 native/bin/bzip2recover native/bin/bunzip2 native/bin/bzcat; do
    [[ -s $output || -L $output ]] || { echo "bzip2 output missing: $output" >&2; exit 1; }
done
mkdir -p build/smoke
printf 'REM Samurai atomic archive smoke test\n' > build/smoke/input
native/bin/bzip2 -c build/smoke/input > build/smoke/input.bz2
native/bin/bzip2 -dc build/smoke/input.bz2 > build/smoke/output
cmp build/smoke/input build/smoke/output
echo 'PASS bzip2'
echo 'REM_STAGE|bzip2-samu|finish'
