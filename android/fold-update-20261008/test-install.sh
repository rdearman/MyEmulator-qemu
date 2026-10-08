#!/bin/bash
# Host regression: use an existing stopped 10 GiB test image; never modify it.
# Usage: bash test-install.sh /path/to/rootfs.ext4 /tmp/rem-update-20261008-v2
set -euo pipefail
image=$1
package=$2
here=$(cd "${0%/*}" && pwd)
repo=$(cd "$here/../.." && pwd)
work=$(mktemp -d /tmp/rem-fold-update-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
export TMPDIR=/tmp
kit=/home/dev/rem-native-userland
cp --sparse=always "$image" "$work/rootfs.ext4"
cp "$package/payload/vmlinux" "$work/vmlinux"
mkdir "$work/base"
git -C "$repo/toolchain/chibicc-rem" archive 90d1f7f199cc55b13c7fdb5839d1409806633fdb |
    tar -x -C "$work/base"
if [ -n "${BASE_PATCH:-}" ]; then
    cp "$BASE_PATCH" "$work/base.patch"
else
    git -C "$repo" show e962d91f238476cc92669e5e514b12075dca0136:toolchain/native-userland/patches/chibicc-rem-native.patch > "$work/base.patch"
fi
[ "$(sha256sum "$work/base.patch" | cut -d' ' -f1)" = b235638b86fb58fcbc9f192b4d460c9ec281f19212dbff1585f4639f6d133f97 ] ||
    { echo "baseline patch mismatch" >&2; exit 1; }
patch -s -p1 -d "$work/base" < "$work/base.patch"
cp "$work/base/parse.c" "$work/local-parse.c"
sed -i '/Node \*expr = init->children\[mem->idx\]->expr;/,+3s/break;/continue;/' "$work/local-parse.c"
printf '\nenum { FOLD_LOCAL_PARSER_CHANGE = 1 };\n' >> "$work/local-parse.c"
put() {
    debugfs -w -R "rm $kit/bootstrap/chibicc/$2" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -w -R "write $1 $kit/bootstrap/chibicc/$2" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -w -R "sif $kit/bootstrap/chibicc/$2 mode 0100644" "$work/rootfs.ext4" >/dev/null 2>&1
}
dump() {
    debugfs -R "dump $kit/bootstrap/chibicc/$1 $2" "$work/rootfs.ext4" >/dev/null 2>&1
}
put "$work/base/codegen.c" codegen.c
put "$work/local-parse.c" parse.c
if sh "$package/install.sh" verify "$work/rootfs.ext4" > "$work/before-verify.log" 2>&1; then
    echo "FAIL incomplete compiler sources reported VERIFY_OK" >&2
    exit 1
fi
grep -q '^VERIFY_INCOMPLETE$' "$work/before-verify.log"
echo "PASS verification rejects an incomplete local ABI update"
sh "$package/install.sh" install "$work/rootfs.ext4" > "$work/install.log" 2>&1
dump parse.c "$work/merged-parse.c"
grep -q 'FOLD_LOCAL_PARSER_CHANGE = 1' "$work/merged-parse.c"
grep -q 'if (rty->kind == TY_STRUCT || rty->kind == TY_UNION)' "$work/merged-parse.c"
sh "$package/install.sh" install "$work/rootfs.ext4" > "$work/rerun.log" 2>&1
sh "$package/install.sh" verify "$work/rootfs.ext4" > "$work/verify.log" 2>&1
grep -q '^VERIFY_OK$' "$work/verify.log"
echo "PASS local parser fix merged, preserved, verified and idempotent"
sh "$package/install.sh" rollback "$work/rootfs.ext4" > "$work/rollback.log" 2>&1
dump parse.c "$work/restored-parse.c"
dump codegen.c "$work/restored-codegen.c"
cmp "$work/local-parse.c" "$work/restored-parse.c"
cmp "$work/base/codegen.c" "$work/restored-codegen.c"
echo "PASS rollback restores exact pre-update compiler sources"
sed 's/uint64_t mask = (1L << mem->bit_width) - 1;/uint64_t mask = 123;/' \
    "$work/local-parse.c" > "$work/conflict-parse.c"
put "$work/conflict-parse.c" parse.c
if sh "$package/install.sh" install "$work/rootfs.ext4" > "$work/conflict.log" 2>&1; then
    echo "FAIL conflicting parser was accepted" >&2
    exit 1
fi
grep -q 'compiler source conflicts' "$work/conflict.log"
dump codegen.c "$work/unmodified-codegen.c"
dump parse.c "$work/unmodified-parse.c"
cmp "$work/base/codegen.c" "$work/unmodified-codegen.c"
cmp "$work/conflict-parse.c" "$work/unmodified-parse.c"
test ! -e "$work/update-20261008-v2-backup"
echo "PASS conflicting parser aborts before either ABI source is written"
