#!/bin/bash
set -Eeuo pipefail
ROOT=$PWD
export REM_USERLAND_ROOT=$ROOT
cd "$ROOT"

CHECKPOINTS=(
    .bootstrap-integer.done .bootstrap-float.done .bootstrap-decimal.done
    .libc-repair.done .compiler-selfbuild.done .compiler-porting.done
    .bootstrap-tools.done .bearssl.done .curl.done .coreutils.done
    .dropbear.done .findutils.done .grep.done .sed.done .tar.done
    .gzip.done .bzip2.done .xz.done .diffutils.done .gawk.done
    .ncurses.done .vim.done .acceptance.done
)

if [[ ${1:-} == --clean-checkpoints ]]; then
    for marker in "${CHECKPOINTS[@]}"; do
        rm -f -- "$marker" "${marker%.done}.inprogress"
    done
    rm -f -- BUILD-ACCEPTED
    echo 'PASS removed native build checkpoints only; source and build artefacts were kept.'
    exit 0
elif [[ $# -ne 0 ]]; then
    echo "Usage: bash ./build-all-native.sh [--clean-checkpoints]" >&2
    exit 2
fi

mkdir -p logs
OVERALL_LOG=logs/build-all-native.log
report() {
    printf '%s\n' "$*"
    printf '%s\n' "$*" >> "$OVERALL_LOG"
}
run_logged() {
    local log=$1; shift
    "$@" 2>&1 | tee "$log" | tee -a "$OVERALL_LOG"
}
trap 'status=$?; message="FAIL native build (exit $status); resume with: bash ./build-all-native.sh"; echo "$message" >&2; echo "$message" >> "$OVERALL_LOG"; exit "$status"' ERR

BEARSSL_BUILDER=$ROOT/packages/bearssl-0.6/build-native.sh

has_files() {
    local file
    for file in "$@"; do [[ -s $file ]] || return 1; done
}

has_execs() {
    local file
    for file in "$@"; do executable_mode_valid "$file" || return 1; done
}

# Bash's -x/access(2) test is false for REM executables on the Fold kernel.
# Read the ext4 mode bits directly instead of treating a valid executable as
# stale and rebuilding its completed stage.
executable_mode_valid() {
    local file=$1 mode
    [[ -s $file ]] || return 1
    mode=$(stat -c '%a' "$file") || return 1
    [[ $mode =~ ^[0-7]{3,4}$ ]] || return 1
    (( (8#$mode & 0111) != 0 ))
}

compiler_aggregate_smoke() {
    local output=native-tests/checkpoint/aggregate-abi
    executable_mode_valid "$CC" && [[ -s tests/aggregate-abi.c ]] || return 1
    mkdir -p "${output%/*}"
    rm -f -- "$output"
    CHIBICC_REM_LINKER_SCRIPT=/usr/lib/myemulator2-user.ld \
        CHIBICC_REM_CRTDIR=/usr/lib CHIBICC_REM_GCCLIB=/usr/lib \
        "$CC" tests/aggregate-abi.c -o "$output" || return 1
    "$output"
}

compiler_porting_provenance_valid() {
    executable_mode_valid "$CC" && [[ -s logs/compiler-porting.compiler.sha256 ]] &&
        [[ $(cat logs/compiler-porting.compiler.sha256) == "$(sha256sum "$CC" | cut -d' ' -f1)" ]]
}

bootstrap_checkpoints_valid() {
    local marker
    for marker in .bootstrap-integer.done .bootstrap-float.done .bootstrap-decimal.done \
        .libc-repair.done .compiler-selfbuild.done .compiler-porting.done .bootstrap-tools.done; do
        [[ -f $marker ]] || return 1
    done
    has_execs bootstrap/native/chibicc-integer bootstrap/native/chibicc-float \
        bootstrap/native/chibicc-decimal bootstrap/native/chibicc bootstrap/native/chibicc-selfbuilt \
        bootstrap/tools/native/expr bootstrap/tools/native/nc bootstrap/tools/native/rm \
        bootstrap/tools/native/cp bootstrap/tools/native/wc bootstrap/tools/native/dirname \
        bootstrap/tools/native/basename || return 1
    has_files bootstrap/native/include/stddef.h bootstrap/compiler-rt-15.0.7.src/native/ieee-runtime.o \
        bootstrap/musl/native/libc.a bootstrap/musl/native/librem-runtime.a \
        bootstrap/musl/native/rem-runtime.o bootstrap/musl/native/native_syscall.o \
        native-tests/porting/gcc-caller native-tests/porting/chibicc-caller \
        native-tests/porting/hashmap-tombstone logs/compiler-porting.log \
        logs/compiler-porting.compiler.sha256 bootstrap/native/chibicc-selfbuilt.provenance || return 1
    grep -q '^COMPILER_REQUIRED_PORTING_TESTS_OK$' logs/compiler-porting.log &&
        compiler_porting_provenance_valid
}

plan_outputs_valid() {
    local package=$1 buildfile="packages/$1/native-build/build.ninja" line target output found=0
    [[ -s $buildfile ]] || return 1
    while IFS= read -r line; do
        [[ $line == default\ * ]] || continue
        found=1
        for target in ${line#default }; do
            output="packages/$package/native-build/$target"
            [[ -s $output ]] || return 1
            case $target in ../native/bin/*) executable_mode_valid "$output" || return 1 ;; esac
        done
    done < "$buildfile"
    [[ $found == 1 ]]
}

valid_stage() {
    case $1 in
        compiler-porting)
            has_execs native-tests/porting/gcc-caller native-tests/porting/chibicc-caller \
                native-tests/porting/hashmap-tombstone &&
                grep -q '^COMPILER_REQUIRED_PORTING_TESTS_OK$' logs/compiler-porting.log &&
                compiler_porting_provenance_valid ;;
        bootstrap-tools)
            has_execs bootstrap/tools/native/expr bootstrap/tools/native/nc \
                bootstrap/tools/native/rm bootstrap/tools/native/cp bootstrap/tools/native/wc \
                bootstrap/tools/native/dirname bootstrap/tools/native/basename ;;
        bearssl)
            has_files packages/bearssl-0.6/build/libbearssl.a native-deps/lib/libbearssl.a \
                native-deps/include/bearssl.h ;;
        curl) plan_outputs_valid curl-8.12.1 ;;
        coreutils) plan_outputs_valid coreutils-9.7 ;;
        dropbear)
            plan_outputs_valid dropbear-2024.86 && executable_mode_valid packages/dropbear-2024.86/native/bin/ssh ;;
        findutils) plan_outputs_valid findutils-4.10.0 ;;
        grep) plan_outputs_valid grep-3.11 ;;
        sed) plan_outputs_valid sed-4.9 ;;
        tar) plan_outputs_valid tar-1.35 ;;
        gzip) plan_outputs_valid gzip-1.13 ;;
        bzip2)
            has_execs packages/bzip2-1.0.8/native/bin/bzip2 &&
                has_execs packages/bzip2-1.0.8/native/bin/bzip2recover \
                    packages/bzip2-1.0.8/native/bin/bunzip2 packages/bzip2-1.0.8/native/bin/bzcat &&
                has_files packages/bzip2-1.0.8/native/libbz2.a ;;
        xz)
            plan_outputs_valid xz-5.6.3 && has_execs packages/xz-5.6.3/native/bin/unxz \
                packages/xz-5.6.3/native/bin/xzcat ;;
        diffutils) plan_outputs_valid diffutils-3.10 ;;
        gawk) plan_outputs_valid gawk-5.3.1 ;;
        ncurses) plan_outputs_valid ncurses-6.5 ;;
        vim) plan_outputs_valid vim-9.1.0000 ;;
        acceptance)
            [[ -s BUILD-ACCEPTED ]] && grep -q '^NATIVE_USERLAND_ACCEPTANCE_OK$' logs/acceptance.log ;;
        *) return 1 ;;
    esac
}

run_stage() {
    local name=$1 marker=$2 log=$3 progress=${2%.done}.inprogress; shift 3
    if [[ -e $marker ]]; then
        if valid_stage "$name"; then
            report "SKIP $name (already complete)"
            return 0
        fi
        report "RESUME $name (stale checkpoint or missing output; rebuilding stage)"
        rm -f -- "$marker"
    elif [[ -e $progress ]]; then
        report "RESUME $name (previous command was interrupted; checking outputs is not enough to skip)"
        rm -f -- "$progress"
    elif valid_stage "$name"; then
        report "SKIP $name (existing outputs verified; recording checkpoint)"
        : > "$marker"
        return 0
    fi
    report "REM_STAGE|$name|start"
    rm -f -- "$log"
    : > "$progress"
    run_logged "$log" "$@"
    if [[ $name == compiler-porting ]]; then
        sha256sum "$CC" | cut -d' ' -f1 > logs/compiler-porting.compiler.sha256
    fi
    if ! valid_stage "$name"; then
        report "FAIL $name: command succeeded but expected output verification failed" >&2
        return 1
    fi
    : > "$marker"
    rm -f -- "$progress"
    report "PASS $name"
    report "REM_STAGE|$name|finish"
}

source ./native-env.sh
if bootstrap_checkpoints_valid; then
    for stage in bootstrap-integer bootstrap-float bootstrap-decimal libc-repair \
        compiler-selfbuild compiler-porting bootstrap-tools; do
        report "SKIP $stage (completed checkpoint and outputs verified)"
    done
else
    report 'REM_STAGE|compiler-bootstrap|start'
    run_logged logs/bootstrap.log bash bootstrap/build-native.sh
    report 'PASS compiler-bootstrap (verified stage checkpoints)'
    report 'REM_STAGE|compiler-bootstrap|finish'
fi

run_stage compiler-porting .compiler-porting.done logs/compiler-porting.log \
    bash tests/compiler-porting-native.sh
run_stage bootstrap-tools .bootstrap-tools.done logs/bootstrap-tools.log \
    bash bootstrap/tools/build-native.sh
run_stage bearssl .bearssl.done logs/bearssl.log \
    bash "$BEARSSL_BUILDER"
run_stage curl .curl.done logs/curl.log \
    bash packages/curl-8.12.1/build-native.sh
run_stage coreutils .coreutils.done logs/coreutils.log \
    bash packages/coreutils-9.7/build-native.sh

for package in dropbear-2024.86 findutils-4.10.0 grep-3.11 sed-4.9 tar-1.35 \
    gzip-1.13 bzip2-1.0.8 xz-5.6.3 diffutils-3.10 gawk-5.3.1 \
    ncurses-6.5 vim-9.1.0000; do
    stage=${package%%-*}
    case $package in dropbear-*) stage=dropbear ;; findutils-*) stage=findutils ;; esac
    marker=.$stage.done
    run_stage "$stage" "$marker" "logs/$package.log" bash "packages/$package/build-native.sh"
done

if valid_stage acceptance; then
    if [[ -e .acceptance.done ]]; then
        report 'SKIP acceptance (already complete)'
    else
        report 'SKIP acceptance (existing outputs verified; recording checkpoint)'
        : > .acceptance.done
    fi
else
    rm -f .acceptance.done BUILD-ACCEPTED
    report 'REM_STAGE|acceptance|start'
    : > .acceptance.inprogress
    run_logged logs/acceptance.log bash acceptance-native.sh
    grep -q '^NATIVE_USERLAND_ACCEPTANCE_OK$' logs/acceptance.log
    printf '%s\n' "$REM_PREFIX" > BUILD-ACCEPTED
    valid_stage acceptance
    : > .acceptance.done
    rm -f .acceptance.inprogress
    report 'PASS acceptance'
    report 'REM_STAGE|acceptance|finish'
fi
report 'NATIVE_USERLAND_BUILD_ACCEPTED'
