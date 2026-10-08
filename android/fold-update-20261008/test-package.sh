#!/bin/bash
# Host-only release-gate tests; synthetic logs are NOT REM execution evidence.
# Usage: bash test-package.sh /path/to/verified/chibicc-update-seed
set -euo pipefail
export CHIBICC_SEED=${1:?provide the existing verified REM-native seed}
here=$(cd "${0%/*}" && pwd)
work=$(mktemp -d /tmp/rem-fold-package-test.XXXXXX)
cleanup() {
    result=$?
    if [ "$result" = 0 ]; then
        rm -rf "$work"
    else
        echo "FAIL package regression; preserved diagnostics: $work" >&2
    fi
}
trap cleanup EXIT
markers=(
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
printf '%s\n' SYNTHETIC_HOST_UNIT_FIXTURE_NOT_REM_EXECUTION "${markers[@]}" > "$work/complete.log"
for marker in "${markers[@]}"; do
    grep -Fvx "$marker" "$work/complete.log" > "$work/missing.log"
    if QUALIFICATION_LOG="$work/missing.log" OUT="$work/rejected" \
        bash "$here/make-package.sh" > "$work/rejected.log" 2>&1; then
        echo "FAIL missing marker accepted: $marker" >&2
        exit 1
    fi
    grep -Fqx "qualification marker missing: $marker" "$work/rejected.log"
    test ! -e "$work/rejected"
done
echo "PASS all 13 individual missing release markers rejected"
for fault in MYEMU_BAD_USER_FAULT "Kernel panic - synthetic fixture" "REM_QA_DONE|1"; do
    { cat "$work/complete.log"; printf '%s\n' "$fault"; } > "$work/fault.log"
    if QUALIFICATION_LOG="$work/fault.log" OUT="$work/rejected" \
        bash "$here/make-package.sh" > "$work/rejected.log" 2>&1; then
        echo "FAIL fault/failed exit accepted: $fault" >&2
        exit 1
    fi
    grep -Fqx "qualification log contains a fault or failed guest result" "$work/rejected.log"
    test ! -e "$work/rejected"
done
echo "PASS user fault, kernel panic and failed guest exit rejected"
synthetic="$work/synthetic-only/rem-update-20261008-v2"
candidate="$work/candidate/rem-update-20261008-v2"
QUALIFICATION_LOG="$work/complete.log" OUT="$synthetic" \
    bash "$here/make-package.sh" > "$work/complete-build.log" 2>&1
(cd "$synthetic" && sha256sum -c payload.sha256 > "$work/complete-checksums.log")
for marker in "${markers[@]}"; do
    grep -Fqx "$marker" "$synthetic/MANIFEST.txt"
done
echo "PASS host-only synthetic marker acceptance and checksums (NOT REM execution)"
bash "$here/archive-package.sh" "$synthetic" "$work/SYNTHETIC-NOT-FOR-TRANSFER.tar.gz" \
    > "$work/archive.log" 2>&1
find "$synthetic" -exec touch -m -t 202610090101 {} +
bash "$here/archive-package.sh" "$synthetic" "$work/SYNTHETIC-NOT-FOR-TRANSFER-second.tar.gz" \
    > "$work/archive-second.log" 2>&1
cmp "$work/SYNTHETIC-NOT-FOR-TRANSFER.tar.gz" "$work/SYNTHETIC-NOT-FOR-TRANSFER-second.tar.gz"
echo "PASS normalized archive reproducibility across changed mtimes, extracted hashes and archive SHA-256 (HOST-ONLY)"
for file in chibicc-update-seed vmlinux; do
    mismatch="$work/mismatched-$file/rem-update-20261008-v2"
    mkdir -p "${mismatch%/*}"
    cp -a "$synthetic" "$mismatch"
    printf 'deliberately mismatched host-only fixture\n' > "$mismatch/payload/$file"
    (cd "$mismatch" &&
        { grep -Fv "  payload/$file" payload.sha256; sha256sum "payload/$file"; } > new.sha256 &&
        mv new.sha256 payload.sha256)
    if bash "$here/archive-package.sh" "$mismatch" "$work/rejected.tar.gz" \
        > "$work/mismatched.log" 2>&1; then
        echo "FAIL mismatched tested binary accepted: $file" >&2
        exit 1
    fi
    grep -Fqx "release input differs from REM-tested binary: $file" "$work/mismatched.log"
    test ! -e "$work/rejected.tar.gz"
done
echo "PASS mismatched seed and kernel rejected despite resealed file checksums"
printf 'uncovered fixture\n' > "$synthetic/uncovered.txt"
if bash "$here/archive-package.sh" "$synthetic" "$work/rejected.tar.gz" \
    > "$work/uncovered.log" 2>&1; then
    echo "FAIL unchecksummed package file accepted" >&2
    exit 1
fi
grep -Fqx "package file inventory differs from checksummed manifest" "$work/uncovered.log"
test ! -e "$work/rejected.tar.gz"
echo "PASS complete checksum inventory enforced"
QUALIFICATION_LOG= OUT="$candidate" \
    bash "$here/make-package.sh" > "$work/candidate-build.log" 2>&1
grep -Fqx "Full REM execution qualification: PENDING; candidate, NOT ready for transfer." \
    "$candidate/MANIFEST.txt"
(cd "$candidate" && sha256sum -c payload.sha256 > "$work/candidate-checksums.log")
if bash "$here/archive-package.sh" "$candidate" "$work/rejected.tar.gz" \
    > "$work/pending-archive.log" 2>&1; then
    echo "FAIL pending candidate archived for release" >&2
    exit 1
fi
grep -Fqx "package is not fully REM-qualified; no release archive created" "$work/pending-archive.log"
test ! -e "$work/rejected.tar.gz"
echo "PASS pending candidate cannot produce a release archive"
if QUALIFICATION_LOG= OUT="$candidate" \
    bash "$here/make-package.sh" > "$work/overwrite.log" 2>&1; then
    echo "FAIL existing package output overwritten" >&2
    exit 1
fi
grep -Fq "output already exists and is not an empty directory:" "$work/overwrite.log"
(cd "$candidate" && sha256sum -c payload.sha256 > "$work/preserved-checksums.log")
echo "PASS pending candidate labeled and nonempty package output preserved"
