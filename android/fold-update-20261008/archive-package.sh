#!/bin/bash
# Create a reproducible release archive only from a qualified, verified package.
# Usage: bash archive-package.sh PACKAGE_DIRECTORY /tmp/REM-FOLD-update-20261008-v2.tar.gz
set -euo pipefail
package=$(cd "${1:?provide the qualified package directory}" && pwd)
archive=${2:?provide a new archive output path}
[ "${package##*/}" = rem-update-20261008-v2 ] ||
    { echo "unexpected package directory name: $package" >&2; exit 1; }
for output in "$archive" "$archive.sha256" "$archive.manifest.txt"; do
    [ ! -e "$output" ] && [ ! -L "$output" ] ||
        { echo "refusing existing release output: $output" >&2; exit 1; }
done
work=$(mktemp -d /tmp/rem-fold-archive.XXXXXX)
cleanup() {
    result=$?
    if [ "$result" = 0 ]; then
        rm -rf "$work"
    else
        echo "FAIL release archive; preserved diagnostics: $work" >&2
    fi
}
trap cleanup EXIT
(cd "$package" && sha256sum -c payload.sha256 > "$work/checksums.log") ||
    { cat "$work/checksums.log" >&2; echo "package checksum verification failed" >&2; exit 1; }
(cd "$package" && find . -type f ! -path ./payload.sha256 -printf '%P\n' | LC_ALL=C sort) > "$work/files"
sed -n 's/^[0-9a-f]\{64\}  //p' "$package/payload.sha256" | LC_ALL=C sort > "$work/covered"
cmp -s "$work/files" "$work/covered" ||
    { echo "package file inventory differs from checksummed manifest" >&2; exit 1; }
find "$package" ! -type d ! -type f -print > "$work/nonregular"
if [ -s "$work/nonregular" ]; then
    echo "package contains unexpected nonregular files" >&2
    exit 1
fi
grep -Fqx "Full REM execution qualification: PASS" "$package/MANIFEST.txt" ||
    { echo "package is not fully REM-qualified; no release archive created" >&2; exit 1; }
for marker in "PASS aggregate-small-return (seed)" \
    "PASS aggregate-small-return (stage1)" "PASS aggregate-small-return (stage2)" \
    COMPILER_REQUIRED_PORTING_TESTS_OK COMPILER_REQUIRED_SELFREBUILD_OK \
    SAMURAI_BUILD_OK SAMURAI_DRY_RUN_OK SAMURAI_NATIVE_BUILD_OK \
    SAMURAI_INCREMENTAL_REBUILD_OK SAMURAI_CLEAN_OK SAMURAI_NATIVE_EXECUTION_OK \
    REM_UPDATE_VERIFY_OK "REM_QA_DONE|0"; do
    grep -Fqx "$marker" "$package/MANIFEST.txt" ||
        { echo "qualification manifest lacks expected execution marker: $marker" >&2; exit 1; }
done
seed_sha=6b28c165cb94b7411a63b7e74932845b11b8e30ea2bd6882b9c7bf1348a2ac44
kernel_sha=c8b394e333fdc8ad3d399e10a3723daad194dfbfbb56e3be74a573fa3d179782
for input in "chibicc-update-seed:$seed_sha" "vmlinux:$kernel_sha"; do
    file=${input%%:*}
    expected=${input#*:}
    actual=$(sha256sum "$package/payload/$file" | cut -d' ' -f1)
    [ "$actual" = "$expected" ] ||
        { echo "release input differs from REM-tested binary: $file" >&2; exit 1; }
done
for copy in first second; do
    tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
        --mode='u+rwX,go+rX,go-w' \
        -C "${package%/*}" -cf - rem-update-20261008-v2 |
        gzip -n > "$work/$copy.tar.gz"
done
cmp "$work/first.tar.gz" "$work/second.tar.gz"
mkdir "$work/extracted"
tar -xzf "$work/first.tar.gz" -C "$work/extracted"
(cd "$work/extracted/rem-update-20261008-v2" &&
    sha256sum -c payload.sha256 > "$work/extracted-checksums.log")
(cd "$work/extracted/rem-update-20261008-v2" &&
    find . -type f ! -path ./payload.sha256 -printf '%P\n' | LC_ALL=C sort) > "$work/extracted-files"
cmp "$work/files" "$work/extracted-files"
(set -C; cat "$work/first.tar.gz" > "$archive")
(cd "$(dirname "$archive")" && sha256sum "$(basename "$archive")") > "$work/archive.sha256"
{
    cat "$package/MANIFEST.txt"
    echo "Archive reproducibility: PASS (two byte-identical normalized archives)"
    echo "Extracted archive payload checksums: PASS"
    echo "Archive bytes: $(wc -c < "$archive" | tr -d ' ')"
    cat "$work/archive.sha256"
} > "$work/archive.manifest.txt"
(set -C; cat "$work/archive.sha256" > "$archive.sha256")
(set -C; cat "$work/archive.manifest.txt" > "$archive.manifest.txt")
(cd "$(dirname "$archive")" && sha256sum -c "$(basename "$archive").sha256")
echo "RELEASE_ARCHIVE_REPRODUCIBLE"
echo "Archive: $archive"
echo "Manifest: $archive.manifest.txt"
cat "$archive.sha256"
