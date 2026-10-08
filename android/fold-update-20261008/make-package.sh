#!/bin/bash
# Assemble the small REM Fold update package (rem-update-20261008) in /tmp.
# Inputs come from this repository plus build outputs:
#   BUSYBOX  BusyBox 1.37.0 built with toolchain/userspace/rem-sysv/busybox-1.37.0-rem-20261008.config
#            and the fixed GCC + toolchain/flight-kit-runtime (default /tmp/remqa/bb-new/busybox)
#   BUSYBOX_LINKS  matching busybox.links (default /tmp/remqa/bb-new/busybox.links)
#   VMLINUX  kernel built from linux/arch/myemulator2 with the phone network config
#            (default /tmp/remqa/linux-netbuild/vmlinux)
#   SAMURAI_TARBALL  samurai-531ba700.tar.gz from toolchain/native-userland/sources.env
#   BASE_PATCH  chibicc-rem-native.patch as shipped in REM-FOLD-BUILD-SOURCE-20261007
#               (SHA-256 b235638b...; default: pinned original kit revision)
#   CHIBICC_SEED  REM-native compiler built by build-seed.sh and executed in REM
#   QUALIFICATION_LOG  successful full REM serial log; omitted for candidate-only packages
set -euo pipefail
here=$(cd "${0%/*}" && pwd)
repo=$(cd "$here/../.." && pwd)
out=${OUT:-/tmp/rem-update-20261008-v2}
BUSYBOX=${BUSYBOX:-/tmp/remqa/bb-new/busybox}
BUSYBOX_LINKS=${BUSYBOX_LINKS:-/tmp/remqa/bb-new/busybox.links}
VMLINUX=${VMLINUX:-/tmp/remqa/linux-netbuild/vmlinux}
SAMURAI_TARBALL=${SAMURAI_TARBALL:-/tmp/rem-native-userland-downloads/samurai-531ba700.tar.gz}
CHIBICC_SEED=${CHIBICC_SEED:?set CHIBICC_SEED to the verified REM-native bootstrap compiler}
K=/home/dev/rem-native-userland
base_patch_sha=b235638b86fb58fcbc9f192b4d460c9ec281f19212dbff1585f4639f6d133f97
chibicc_base=90d1f7f199cc55b13c7fdb5839d1409806633fdb
samurai_top=samurai-531ba700c5fbbfd3d8e3ed91226d0c57134a6fb3
s() { sha256sum "$1" | cut -d' ' -f1; }

work=$(mktemp -d /tmp/rem-update-make.XXXXXX)
trap 'rm -rf "$work"' EXIT
if [ -n "${BASE_PATCH:-}" ]; then cp "$BASE_PATCH" "$work/base.patch"
else git -C "$repo" show e962d91f238476cc92669e5e514b12075dca0136:toolchain/native-userland/patches/chibicc-rem-native.patch > "$work/base.patch"; fi
[ "$(s "$work/base.patch")" = "$base_patch_sha" ] || { echo "baseline patch mismatch" >&2; exit 1; }
qualification_markers=(
    "FOLD_LIBC_INPUT_SHA256=0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186"
    "FOLD_LIBC_UPDATED_SHA256=269b1d6b5a15a3e2b3c4f8fe48dc0f9f8b5922e2c90937586ec7167d22a1c7c2"
    "PASS aggregate-small-return (seed)"
    "PASS aggregate-small-return (stage1)"
    "PASS aggregate-small-return (stage2)"
    COMPILER_REQUIRED_PORTING_TESTS_OK
    COMPILER_REQUIRED_SELFREBUILD_OK
    SAMURAI_BUILD_OK
    SAMURAI_DRY_RUN_OK
    SAMURAI_NATIVE_BUILD_OK
    SAMURAI_INCREMENTAL_REBUILD_OK
    SAMURAI_CLEAN_OK
    SAMURAI_NATIVE_EXECUTION_OK
    REM_UPDATE_VERIFY_OK
    "REM_QA_DONE|0"
)
if [ -n "${QUALIFICATION_LOG:-}" ]; then
    tr -d '\r' < "$QUALIFICATION_LOG" > "$work/qualification.log"
    for marker in "${qualification_markers[@]}"; do
        grep -Fqx "$marker" "$work/qualification.log" ||
            { echo "qualification marker missing: $marker" >&2; exit 1; }
    done
    if grep -Eiq 'MYEMU_BAD_USER_FAULT|Kernel panic|Out of memory|Killed process|oom-kill|segmentation fault|illegal instruction|bus error|stack smashing detected|floating point exception|assertion .* failed|REM_QA_DONE\|[1-9]' "$work/qualification.log"; then
        echo "qualification log contains a fault or failed guest result" >&2
        exit 1
    fi
fi
for v in base new; do
    mkdir -p "$work/$v"
    git -C "$repo/toolchain/chibicc-rem" archive "$chibicc_base" | tar -x -C "$work/$v"
done
patch -s -p1 -d "$work/base" < "$work/base.patch"
patch -s -p1 -d "$work/new" < "$repo/toolchain/native-userland/patches/chibicc-rem-native.patch"
for f in codegen.c parse.c; do
    cmp "$work/new/$f" "$repo/toolchain/chibicc-rem/$f"
done

if [ -e "$out" ]; then
    [ -d "$out" ] && [ -z "$(ls -A "$out")" ] ||
        { echo "output already exists and is not an empty directory: $out" >&2; exit 1; }
fi
mkdir -p "$out/payload/samurai" "$out/guest"
cp "$here/install.sh" "$out/install.sh"
cp "$here/README.txt" "$out/README.txt"
cp "$here/verify.sh" "$out/guest/verify.sh"
cp "$here/verify.sh" "$out/payload/verify.sh"
cp "$BUSYBOX" "$out/payload/busybox"
cp "$repo/toolchain/userspace/rem-sysv/etc/init.d/rcS" "$out/payload/rcS"
cp "$repo/toolchain/flight-kit-runtime/lib/libc.a" "$repo/toolchain/flight-kit-runtime/lib/crt1.o" "$out/payload/"
cp "$VMLINUX" "$out/payload/vmlinux"
cp "$work/new/codegen.c" "$work/new/parse.c" "$out/payload/"
for f in codegen.c parse.c; do
    diff -U1 --label "a/$f" --label "b/$f" "$work/base/$f" "$work/new/$f" > "$out/payload/$f.diff" || [ "$?" = 1 ]
done
cp "$repo/toolchain/native-userland/patches/chibicc-rem-native.patch" "$out/payload/"
cp "$repo/toolchain/native-userland/kit/bootstrap/build-samurai-native.sh" "$out/payload/"
cp "$repo/toolchain/native-userland/kit/bootstrap/rebuild-required-native.sh" "$out/payload/"
cp "$repo/toolchain/tests/native-userland/aggregate-small-return.c" "$out/payload/"
cp "$repo/toolchain/tests/native-userland/compiler-porting-native.sh" "$out/payload/"
cp "$CHIBICC_SEED" "$out/payload/chibicc-update-seed"
tar -xzf "$SAMURAI_TARBALL" -C "$work"
(cd "$work/$samurai_top" && cp *.c *.h LICENSE README.md samu.1 "$out/payload/samurai/")

bb=$(s "$out/payload/busybox")
{
echo "/bin/busybox|busybox|9e7acc1db28a5506215293e3d0af19ab734bcb527d5145b45372c2963ba9e31d|$bb|0|"
echo "/bin/rem-transfer-busybox|busybox|58f2d1f4a353ae71abedb3572debc7298a494e764d6491a3716748201a8bf648|$bb|0|"
echo "/bin/busybox-human-baseline|busybox|1d8ae8c351cedffdea2473c3d8108725be39c4f6899751d92af931044ad4d1b5|$bb|0|"
echo "/etc/init.d/rcS|rcS|904951e51ca7bc937074da87cfc33dbb84ab3c19670b46ea40526ffa31bfa6fa|$(s "$out/payload/rcS")|0|"
echo "/usr/lib/libc.a|libc.a|30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924|$(s "$out/payload/libc.a")|0|"
echo "/usr/lib/crt1.o|crt1.o|9951b664ec350b8ae34c30be9824a42b3a76253978515d674bf1d33b346a4697|$(s "$out/payload/crt1.o")|0|"
echo "$K/bootstrap/musl/native/libc.a|libc.a|NEW|$(s "$out/payload/libc.a")|0|0644"
echo "$K/bootstrap/chibicc/codegen.c|codegen.c|$(s "$work/base/codegen.c")|$(s "$out/payload/codegen.c")|1|"
echo "$K/bootstrap/chibicc/parse.c|parse.c|$(s "$work/base/parse.c")|$(s "$out/payload/parse.c")|1|"
echo "$K/patches/chibicc-rem-native.patch|chibicc-rem-native.patch|$base_patch_sha|$(s "$out/payload/chibicc-rem-native.patch")|0|"
echo "$K/bootstrap/build-samurai-native.sh|build-samurai-native.sh|NEW|$(s "$out/payload/build-samurai-native.sh")|0|0755"
echo "$K/bootstrap/rebuild-required-native.sh|rebuild-required-native.sh|NEW|$(s "$out/payload/rebuild-required-native.sh")|0|0755"
echo "$K/bootstrap/native/chibicc-update-seed|chibicc-update-seed|NEW|$(s "$out/payload/chibicc-update-seed")|0|0755"
echo "$K/tests/aggregate-small-return.c|aggregate-small-return.c|NEW|$(s "$out/payload/aggregate-small-return.c")|0|0644"
echo "$K/tests/compiler-porting-native.sh|compiler-porting-native.sh|NEW|$(s "$out/payload/compiler-porting-native.sh")|0|0755"
echo "/home/dev/rem-update-20261008/verify.sh|verify.sh|NEW|$(s "$out/payload/verify.sh")|0|0755"
for f in $(cd "$out/payload/samurai" && ls); do
    echo "$K/packages/samurai/$f|samurai/$f|NEW|$(s "$out/payload/samurai/$f")|0|0644"
done
} > "$out/files.list"
printf '%s\n' "$K/packages/samurai" /home/dev/rem-update-20261008 > "$out/dirs.list"
sed 's#^#/#; s#^//#/#' "$BUSYBOX_LINKS" > "$out/applets.list"
{
    echo "REM Fold incremental update 20261008 v2"
    echo "Target: ~/rem/rootfs.ext4, existing 10 GiB ext4; QEMU must be stopped."
    echo "No image replacement, formatting, resizing, or source-tree replacement."
    echo "Original compiler patch SHA-256: $base_patch_sha"
    echo "REM-native bootstrap seed SHA-256: $(s "$out/payload/chibicc-update-seed")"
    echo "Kernel SHA-256: $(s "$out/payload/vmlinux")"
    echo "Supported actual Fold private libc baseline SHA-256: 0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186"
    echo "This baseline is accepted only at $K/bootstrap/musl/native/libc.a and is replaced in full."
    if [ -n "${QUALIFICATION_LOG:-}" ]; then
        echo "Full REM execution qualification: PASS"
        echo "Qualification serial SHA-256: $(s "$QUALIFICATION_LOG")"
        printf '%s\n' "${qualification_markers[@]}"
    else
        echo "Full REM execution qualification: PENDING; candidate, NOT ready for transfer."
    fi
    echo "Installer regression and archive SHA-256: see adjacent release report."
    echo "Backups: ~/rem/update-20261008-v2-backup; log: ~/rem/update-20261008-v2.log"
    echo "Install: sh rem-update-20261008-v2/install.sh install"
    echo "Verify: sh rem-update-20261008-v2/install.sh verify"
    echo "Rollback (QEMU stopped): sh rem-update-20261008-v2/install.sh rollback"
    echo "Guest verification: sh /home/dev/rem-update-20261008/verify.sh full"
    echo "Payload mapping (guest path | package payload):"
    cut -d'|' -f1,2 "$out/files.list"
} > "$out/MANIFEST.txt"
(cd "$out" && { find payload guest -type f | sort | xargs sha256sum; sha256sum install.sh README.txt MANIFEST.txt files.list dirs.list applets.list; } > payload.sha256)
echo "Package tree: $out"
