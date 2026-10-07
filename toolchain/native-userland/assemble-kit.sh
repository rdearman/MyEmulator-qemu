#!/bin/bash
# Assemble the transferable REM native-userland source kit from verified
# upstream archives plus the REM deltas tracked in this directory.
#
#   assemble-kit.sh [OUTPUT-DIR]
#
# OUTPUT-DIR defaults to $REM_NATIVE_WORK/kit (/tmp/rem-native-userland-work/kit)
# and must not exist.  Archives are read from, or fetched into,
# $REM_NATIVE_DOWNLOADS (default /tmp/rem-native-userland-downloads).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
work=${REM_NATIVE_WORK:-/tmp/rem-native-userland-work}
downloads=${REM_NATIVE_DOWNLOADS:-/tmp/rem-native-userland-downloads}
out=${1:-$work/kit}
chibicc_base=90d1f7f199cc55b13c7fdb5839d1409806633fdb

if [ -e "$out" ]; then
    echo "Refusing to replace existing $out" >&2
    exit 1
fi
mkdir -p "$downloads" "$out"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/rem-kit-extract.XXXXXX")
trap 'rm -rf "$scratch"' EXIT

while IFS='|' read -r dest archive top url sha; do
    case $dest in ''|'#'*) continue ;; esac
    if [ ! -f "$downloads/$archive" ]; then
        curl -fL --retry 3 -o "$downloads/$archive.part" "$url"
        mv "$downloads/$archive.part" "$downloads/$archive"
    fi
    echo "$sha  $downloads/$archive" | sha256sum -c --quiet -
    tar -xf "$downloads/$archive" -C "$scratch"
    mkdir -p "$(dirname "$out/$dest")"
    mv "$scratch/$top" "$out/$dest"
done < "$here/sources.env"

# Pristine upstream chibicc; prefer the local REM port repository's history.
mkdir -p "$out/bootstrap/chibicc"
if git -C "$repo/toolchain/chibicc-rem" cat-file -e "$chibicc_base^{commit}" 2>/dev/null; then
    git -C "$repo/toolchain/chibicc-rem" archive "$chibicc_base" | tar -x -C "$out/bootstrap/chibicc"
else
    curl -fL "https://github.com/rui314/chibicc/archive/$chibicc_base.tar.gz" |
        tar -xz --strip-components=1 -C "$out/bootstrap/chibicc"
fi

apply() {
    patch -s -E --no-backup-if-mismatch -p1 -d "$out/$1" < "$here/patches/$2"
}
apply bootstrap/chibicc chibicc-rem-native.patch
apply bootstrap/musl musl-rem-native.patch
apply bootstrap/compiler-rt-15.0.7.src compiler-rt-rem.patch
apply bootstrap/make-4.4.1 make-rem.patch
apply bootstrap/ryu ryu-rem.patch
for plan in "$here"/patches/*-rem-native.patch; do
    name=$(basename "$plan" -rem-native.patch)
    [ -d "$out/packages/$name" ] && apply "packages/$name" "$(basename "$plan")"
done
# Patches cannot create empty files.
: > "$out/bootstrap/chibicc/test/rem-wide-return.c"
while read -r path; do
    [ -n "$path" ] && chmod +x "$out/$path"
done < "$here/executable-files.list"

for plan in "$here"/plans/*; do
    cp -pr "$plan" "$out/packages/$(basename "$plan")/rem-native-plan"
done

cp -pr "$here"/kit/. "$out/"
mkdir -p "$out/patches" "$out/tests" "$out/docs"
cp -p "$here"/patches/*.patch "$out/patches/"
cp -pr "$repo/toolchain/tests/native-userland/." "$out/tests/"
cp -p "$repo/docs/CHIBICC_UPSTREAM_REPORT.md" "$repo/docs/REM_CHIBICC_BUGLOG.md" "$out/docs/"
echo "Assembled REM native-userland source kit: $out"
