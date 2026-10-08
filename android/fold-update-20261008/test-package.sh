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
QUALIFICATION_LOG="$work/complete.log" OUT="$work/synthetic-only" \
    bash "$here/make-package.sh" > "$work/complete-build.log" 2>&1
(cd "$work/synthetic-only" && sha256sum -c payload.sha256 > "$work/complete-checksums.log")
for marker in "${markers[@]}"; do
    grep -Fqx "$marker" "$work/synthetic-only/MANIFEST.txt"
done
echo "PASS host-only synthetic marker acceptance and checksums (NOT REM execution)"
QUALIFICATION_LOG= OUT="$work/candidate" \
    bash "$here/make-package.sh" > "$work/candidate-build.log" 2>&1
grep -Fqx "Full REM execution qualification: PENDING; candidate, NOT ready for transfer." \
    "$work/candidate/MANIFEST.txt"
(cd "$work/candidate" && sha256sum -c payload.sha256 > "$work/candidate-checksums.log")
if QUALIFICATION_LOG= OUT="$work/candidate" \
    bash "$here/make-package.sh" > "$work/overwrite.log" 2>&1; then
    echo "FAIL existing package output overwritten" >&2
    exit 1
fi
grep -Fq "output already exists and is not an empty directory:" "$work/overwrite.log"
(cd "$work/candidate" && sha256sum -c payload.sha256 > "$work/preserved-checksums.log")
echo "PASS pending candidate labeled and nonempty package output preserved"
