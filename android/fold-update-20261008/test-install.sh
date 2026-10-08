#!/bin/bash
# Host regression: use an existing stopped 10 GiB test image; never modify it.
# Usage: bash test-install.sh /path/to/rootfs.ext4 /tmp/rem-update-20261008-v2
set -euo pipefail
image=$1
package=$2
fold_libc=${FOLD_LIBC:-}
here=$(cd "${0%/*}" && pwd)
repo=$(cd "$here/../.." && pwd)
work=$(mktemp -d /tmp/rem-fold-update-test.XXXXXX)
cleanup() {
    result=$?
    if [ "$result" = 0 ]; then
        rm -rf "$work"
    else
        echo "FAIL installer regression; preserved diagnostics: $work" >&2
    fi
}
trap cleanup EXIT
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
printf '/* Fold-local source and data must survive the update. */\n' > "$work/local-user-source.c"
put "$work/local-user-source.c" .rem-fold-update-user-data-regression.c
private_libc=$kit/bootstrap/musl/native/libc.a
if [ -n "$fold_libc" ]; then
    [ "$(sha256sum "$fold_libc" | cut -d' ' -f1)" = 0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186 ] ||
        { echo "actual Fold libc fixture hash mismatch" >&2; exit 1; }
    debugfs -w -R "rm $private_libc" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -w -R "write $fold_libc $private_libc" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -w -R "sif $private_libc mode 0100644" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -w -R "sif $private_libc uid 1000" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -w -R "sif $private_libc gid 100" "$work/rootfs.ext4" >/dev/null 2>&1
fi
debugfs -R "dump $private_libc $work/original-private-libc.a" "$work/rootfs.ext4" >/dev/null 2>&1
if [ -n "$fold_libc" ]; then
    cmp "$fold_libc" "$work/original-private-libc.a"
    echo "PASS actual Fold libc fixture bytes verified"
fi
runtime_backups=0
for runtime in bootstrap/native/chibicc-selfbuilt bootstrap/tools/native/samu; do
    key=${runtime##*/}
    debugfs -R "dump $kit/$runtime $work/original-$key" "$work/rootfs.ext4" >/dev/null 2>&1
    if [ -f "$work/original-$key" ]; then
        runtime_backups=$((runtime_backups + 1))
    fi
done
[ "$runtime_backups" -gt 0 ] ||
    { echo "fixture has no native runtime to exercise post-build rollback" >&2; exit 1; }
reject_install() {
    name=$1
    rejected_package=$2
    rejected_image=$3
    message=$4
    if REM_KERNEL=${5:-$work/vmlinux} sh "$rejected_package/install.sh" install "$rejected_image" \
        > "$work/reject-$name.log" 2>&1; then
        echo "FAIL invalid installation accepted: $name" >&2
        exit 1
    fi
    grep -Fq "$message" "$work/reject-$name.log"
    test ! -e "$work/update-20261008-v2-backup"
}
before_guards=$(sha256sum "$work/rootfs.ext4" "$work/vmlinux")
cp -a "$package" "$work/bad-package"
printf '\ncorrupted package fixture\n' >> "$work/bad-package/payload/parse.c"
reject_install corruption "$work/bad-package" "$work/rootfs.ext4" "package payload checksum mismatch"
cp "$package/payload/parse.c" "$work/bad-package/payload/parse.c"
rm "$work/bad-package/payload/codegen.c"
reject_install missing-payload "$work/bad-package" "$work/rootfs.ext4" "package payload checksum mismatch"
cp "$package/payload/codegen.c" "$work/bad-package/payload/codegen.c"
sed '1s/^[0-9a-f]\{64\}/0000000000000000000000000000000000000000000000000000000000000000/' \
    "$package/payload.sha256" > "$work/bad-package/payload.sha256"
reject_install wrong-hash "$work/bad-package" "$work/rootfs.ext4" "package payload checksum mismatch"
reject_install missing-kernel "$package" "$work/rootfs.ext4" "kernel missing:" "$work/missing-vmlinux"
printf 'not a 10 GiB image\n' > "$work/small.ext4"
reject_install wrong-size "$package" "$work/small.ext4" "expected the 10 GiB"
mkdir "$work/remvm"
ln -s "$work/rootfs.ext4" "$work/remvm/rootfs.ext4"
reject_install obsolete-image "$package" "$work/remvm/rootfs.ext4" "refusing obsolete remvm image"
test "$before_guards" = "$(sha256sum "$work/rootfs.ext4" "$work/vmlinux")"
echo "PASS corrupt/missing/wrong-hash payload, missing kernel, wrong size and obsolete image rejected without image/kernel changes"
debugfs -R "dump /usr/lib/libc.a $work/original-libc.a" "$work/rootfs.ext4" >/dev/null 2>&1
debugfs -R "dump $kit/bootstrap/rebuild-required-native.sh $work/original-rebuild.sh" "$work/rootfs.ext4" >/dev/null 2>&1
test -f "$work/original-libc.a"
test -f "$work/original-rebuild.sh"
conflicts="missing-library modified-library modified-helper"
if [ -n "$fold_libc" ]; then
    conflicts="$conflicts modified-private-library misplaced-fold-library"
fi
for conflict in $conflicts; do
    case "$conflict" in
        missing-library)
            guest=/usr/lib/libc.a
            original="$work/original-libc.a"
            debugfs -w -R "rm $guest" "$work/rootfs.ext4" >/dev/null 2>&1
            ;;
        modified-library|modified-helper|modified-private-library|misplaced-fold-library)
            if [ "$conflict" = modified-library ]; then
                guest=/usr/lib/libc.a
                original="$work/original-libc.a"
            elif [ "$conflict" = modified-helper ]; then
                guest=$kit/bootstrap/rebuild-required-native.sh
                original="$work/original-rebuild.sh"
            elif [ "$conflict" = modified-private-library ]; then
                guest=$private_libc
                original="$work/original-private-libc.a"
            else
                guest=/usr/lib/libc.a
                original="$work/original-libc.a"
            fi
            printf 'Fold-local changes must not be overwritten\n' > "$work/local-conflict"
            if [ "$conflict" = misplaced-fold-library ]; then
                cp "$fold_libc" "$work/local-conflict"
            fi
            debugfs -w -R "rm $guest" "$work/rootfs.ext4" >/dev/null 2>&1
            debugfs -w -R "write $work/local-conflict $guest" "$work/rootfs.ext4" >/dev/null 2>&1
            ;;
    esac
    before_conflict=$(sha256sum "$work/rootfs.ext4" "$work/vmlinux")
    reject_install "$conflict" "$package" "$work/rootfs.ext4" "unsupported or missing baseline for $guest"
    test "$before_conflict" = "$(sha256sum "$work/rootfs.ext4" "$work/vmlinux")"
    debugfs -w -R "rm $guest" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -w -R "write $original $guest" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -w -R "sif $guest mode 0100644" "$work/rootfs.ext4" >/dev/null 2>&1
    if [ "$guest" = "$private_libc" ] && [ -n "$fold_libc" ]; then
        debugfs -w -R "sif $guest uid 1000" "$work/rootfs.ext4" >/dev/null 2>&1
        debugfs -w -R "sif $guest gid 100" "$work/rootfs.ext4" >/dev/null 2>&1
    fi
done
echo "PASS missing/modified required library and local helper conflicts abort before any image/kernel mutation"
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
dump .rem-fold-update-user-data-regression.c "$work/user-source-installed.c"
cmp "$work/local-user-source.c" "$work/user-source-installed.c"
echo "PASS local parser fix merged, preserved, verified and idempotent"
debugfs -R "dump $private_libc $work/installed-private-libc.a" "$work/rootfs.ext4" >/dev/null 2>&1
cmp "$package/payload/libc.a" "$work/installed-private-libc.a"
if [ -n "$fold_libc" ]; then
    cmp "$fold_libc" "$work/update-20261008-v2-backup/files/_home_dev_rem-native-userland_bootstrap_musl_native_libc.a"
    echo "PASS actual Fold libc replaced in full and backed up byte-for-byte"
fi
printf '#!/bin/sh\nexit 73\n' > "$work/rebuilt-runtime"
for runtime in bootstrap/native/chibicc-selfbuilt bootstrap/tools/native/samu; do
    key=${runtime##*/}
    if [ -f "$work/original-$key" ]; then
        backup_key=$(printf '%s' "$kit/$runtime" | sed 's#/#_#g')
        cmp "$work/original-$key" "$work/update-20261008-v2-backup/files/$backup_key"
        debugfs -w -R "rm $kit/$runtime" "$work/rootfs.ext4" >/dev/null 2>&1
    else
        for directory in "$kit/bootstrap/tools" "$kit/bootstrap/tools/native"; do
            if ! debugfs -R "stat $directory" "$work/rootfs.ext4" 2>/dev/null | grep -q 'Inode:'; then
                debugfs -w -R "mkdir $directory" "$work/rootfs.ext4" >/dev/null 2>&1
            fi
        done
    fi
    debugfs -w -R "write $work/rebuilt-runtime $kit/$runtime" "$work/rootfs.ext4" >/dev/null 2>&1
    debugfs -R "dump $kit/$runtime $work/rebuilt-$key" "$work/rootfs.ext4" >/dev/null 2>&1
    cmp "$work/rebuilt-runtime" "$work/rebuilt-$key"
done
sh "$package/install.sh" install "$work/rootfs.ext4" > "$work/postbuild-rerun.log" 2>&1
grep -q '^VERIFY_OK$' "$work/postbuild-rerun.log"
sh "$package/install.sh" rollback "$work/rootfs.ext4" > "$work/rollback.log" 2>&1
dump parse.c "$work/restored-parse.c"
dump codegen.c "$work/restored-codegen.c"
cmp "$work/local-parse.c" "$work/restored-parse.c"
cmp "$work/base/codegen.c" "$work/restored-codegen.c"
echo "PASS rollback restores exact pre-update compiler sources"
if [ -f "$work/original-private-libc.a" ]; then
    debugfs -R "dump $private_libc $work/restored-private-libc.a" "$work/rootfs.ext4" >/dev/null 2>&1
    cmp "$work/original-private-libc.a" "$work/restored-private-libc.a"
fi
if [ -n "$fold_libc" ]; then
    debugfs -R "stat $private_libc" "$work/rootfs.ext4" 2>/dev/null > "$work/restored-private-libc.stat"
    grep -Eq 'Mode: *0644' "$work/restored-private-libc.stat"
    grep -Eq 'User: *1000.*Group: *100' "$work/restored-private-libc.stat"
    sh "$package/install.sh" install "$work/rootfs.ext4" > "$work/after-rollback-install.log" 2>&1
    sh "$package/install.sh" verify "$work/rootfs.ext4" > "$work/after-rollback-verify.log" 2>&1
    grep -q '^VERIFY_OK$' "$work/after-rollback-verify.log"
    sh "$package/install.sh" rollback "$work/rootfs.ext4" > "$work/after-rollback-rollback.log" 2>&1
    debugfs -R "dump $private_libc $work/twice-restored-private-libc.a" "$work/rootfs.ext4" >/dev/null 2>&1
    cmp "$fold_libc" "$work/twice-restored-private-libc.a"
    echo "PASS exact Fold libc bytes/ownership/mode restored; reinstall after rollback verified"
fi
for runtime in bootstrap/native/chibicc-selfbuilt bootstrap/tools/native/samu; do
    key=${runtime##*/}
    if [ -f "$work/original-$key" ]; then
        debugfs -R "dump $kit/$runtime $work/restored-$key" "$work/rootfs.ext4" >/dev/null 2>&1
        cmp "$work/original-$key" "$work/restored-$key"
    elif debugfs -R "stat $kit/$runtime" "$work/rootfs.ext4" 2>/dev/null | grep -q 'Inode:'; then
        echo "FAIL rollback kept newly built $runtime" >&2
        exit 1
    fi
done
dump .rem-fold-update-user-data-regression.c "$work/user-source-restored.c"
cmp "$work/local-user-source.c" "$work/user-source-restored.c"
echo "PASS post-build reinstall/rollback restores $runtime_backups original runtimes, removes newly built runtimes, preserves local source/data"
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
