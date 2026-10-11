#!/bin/bash
# Build BearSSL with ordinary Samurai/Ninja timestamp and dependency handling.
set -Eeuo pipefail
USERLAND_ROOT=${REM_USERLAND_ROOT:?REM_USERLAND_ROOT must be exported}
PKG_ROOT=$USERLAND_ROOT/packages/bearssl-0.6
SAMU=${SAMU:-samu}
PATH=$USERLAND_ROOT/bootstrap/tools/native:$USERLAND_ROOT/packages/coreutils-9.7/native/bin:/usr/bin:/bin:$PATH
export PATH
command -v "$SAMU" >/dev/null 2>&1 && [[ -s $PKG_ROOT/build.ninja ]] || {
    echo "BearSSL Samurai inputs are missing under $PKG_ROOT" >&2
    exit 1
}
cd "$PKG_ROOT"
echo 'REM_STAGE|bearssl-samu|start'
# Keep compiler memory pressure bounded on the Fold and REM/QEMU CPU.
"$SAMU" -j1
[[ -s build/libbearssl.a ]] || { echo 'BearSSL archive missing after Samurai' >&2; exit 1; }
ar t build/libbearssl.a >/dev/null
nm -g build/libbearssl.a >/dev/null
mkdir -p "$USERLAND_ROOT/native-deps/include" "$USERLAND_ROOT/native-deps/lib"
for header in inc/*.h; do
    name=${header##*/}
    cp "$header" "$USERLAND_ROOT/native-deps/include/$name.tmp"
    mv -f "$USERLAND_ROOT/native-deps/include/$name.tmp" "$USERLAND_ROOT/native-deps/include/$name"
done
cp build/libbearssl.a "$USERLAND_ROOT/native-deps/lib/libbearssl.a.tmp"
mv -f "$USERLAND_ROOT/native-deps/lib/libbearssl.a.tmp" \
    "$USERLAND_ROOT/native-deps/lib/libbearssl.a"
cat > build/rem-bearssl-link-smoke.c <<'EOF'
#include <bearssl.h>
int main(void)
{
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    return 0;
}
EOF
smoke=build/rem-bearssl-link-smoke
rm -f "$smoke.tmp"
"$USERLAND_ROOT/bootstrap/native/chibicc-selfbuilt" -O0 -DBR_NO_ASM=1 -DBR_INT128=0 \
    -Iinc -Isrc "$smoke.c" "$USERLAND_ROOT/native-deps/lib/libbearssl.a" \
    -o "$smoke.tmp"
mv -f "$smoke.tmp" "$smoke"
"$smoke"
echo 'PASS BearSSL link smoke'
echo 'PASS bearssl'
echo 'REM_STAGE|bearssl-samu|finish'
