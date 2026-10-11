#!/bin/bash
set -Eeuo pipefail
KIT=${REM_USERLAND_ROOT:?REM_USERLAND_ROOT must be exported by build-all-native.sh}
cd "$KIT/bootstrap"
FIXED_CODEGEN_SHA256=68a016b46567589f1a3429097a5229625b4f8a1d38fee7e0519c3ee889991c30
FIXED_PARSE_SHA256=82978ac8fd67803325d29bc8db47997f1ef2f2e550bd155e3dfad18881eedd9f

valid_elf() {
    [[ -s $1 ]] || return 1
    local header
    header=$(od -An -tx1 -N4 "$1") || return 1
    [[ $header == *"7f 45 4c 46"* ]]
}

# Bash's -x test uses access(2), which reports false for executable REM files
# on the Fold kernel even when the mode bits are set. Check the inode mode
# directly; actual compiler smoke tests below still execute the binaries.
valid_executable_mode() {
    local mode
    [[ -s $1 ]] || return 1
    mode=$(stat -c '%a' "$1") || return 1
    [[ $mode =~ ^[0-7]{3,4}$ ]] || return 1
    (( (8#$mode & 0111) != 0 ))
}

valid_compiler() {
    valid_elf "$1" && valid_executable_mode "$1"
}

archive_has_repair_objects() {
    local archive=$1 file member
    local members
    members=$(ar t "$archive" | tr '\n' ' ') || return 1
    for file in crypt_blowfish.o crypt_sha512.o floatscan.o cuserid.o catopen.o \
        __rem_pio2_large.o nftw.o realpath.o syslog.o mbsnrtowcs.o getaddrinfo.o \
        gethostbyname2_r.o getnameinfo.o lookup_name.o lookup_serv.o netlink.o \
        res_send.o resolvconf.o posix_spawn.o system.o glob.o sigset.o fputws.o \
        tempnam.o vfprintf.o fcvt.o strtod.o memmem.o strstr.o sem_open.o \
        timer_create.o wcsftime.o faccessat.o native_syscall.o; do
        case " $members " in *" $file "*) ;; *) return 1 ;; esac
    done
}

compiler_source_valid() {
    [[ -f chibicc/codegen.c && -f chibicc/parse.c ]] &&
        [[ $(sha256sum chibicc/codegen.c | cut -d' ' -f1) == "$FIXED_CODEGEN_SHA256" ]] &&
        [[ $(sha256sum chibicc/parse.c | cut -d' ' -f1) == "$FIXED_PARSE_SHA256" ]]
}

compiler_provenance_valid() {
    local binary_sha
    [[ -s native/chibicc-selfbuilt.provenance ]] || return 1
    binary_sha=$(sha256sum native/chibicc-selfbuilt | cut -d' ' -f1)
    grep -Fqx "source_sha256=$FIXED_CODEGEN_SHA256" native/chibicc-selfbuilt.provenance &&
        grep -Fqx "parse_sha256=$FIXED_PARSE_SHA256" native/chibicc-selfbuilt.provenance &&
        grep -Fqx "compiler_sha256=$binary_sha" native/chibicc-selfbuilt.provenance
}

aggregate_abi_smoke() {
    local compiler=$1 output="$KIT/native-tests/checkpoint/aggregate-abi"
    valid_compiler "$compiler" && [[ -s "$KIT/tests/aggregate-abi.c" ]] || return 1
    mkdir -p "${output%/*}"
    rm -f -- "$output"
    CHIBICC_REM_LIBC="$KIT/bootstrap/musl/native/libc.a" \
        CHIBICC_REM_SOFTFLOAT="$KIT/bootstrap/compiler-rt-15.0.7.src/native/ieee-runtime.o" \
        CHIBICC_REM_LINKER_SCRIPT=/usr/lib/myemulator2-user.ld \
        CHIBICC_REM_CRTDIR=/usr/lib CHIBICC_REM_GCCLIB=/usr/lib \
        "$compiler" "$KIT/tests/aggregate-abi.c" -o "$output" || return 1
    "$output"
}

stage_valid() {
    case $1 in
        bootstrap-integer) valid_compiler native/chibicc-integer && [[ -s native/include/stddef.h ]] ;;
        bootstrap-float) valid_compiler native/chibicc-float && valid_elf compiler-rt-15.0.7.src/native/ieee-runtime.o ;;
        bootstrap-decimal) valid_compiler native/chibicc-decimal && [[ -s musl/native/libc.a ]] ;;
        libc-repair)
            [[ -s musl/native/libc.a && -s musl/native/librem-runtime.a && -s musl/native/rem-runtime.o ]] || return 1
            local file
            for file in crypt/crypt_blowfish crypt/crypt_sha512 internal/floatscan \
                legacy/cuserid locale/catopen math/__rem_pio2_large misc/nftw misc/realpath \
                misc/syslog multibyte/mbsnrtowcs network/getaddrinfo network/gethostbyname2_r \
                network/getnameinfo network/lookup_name network/lookup_serv network/netlink \
                network/res_send network/resolvconf process/posix_spawn process/system regex/glob \
                signal/sigset stdio/fputws stdio/tempnam stdio/vfprintf stdlib/fcvt stdlib/strtod \
                string/memmem string/strstr thread/sem_open time/timer_create time/wcsftime \
                unistd/faccessat; do
                echo "VERIFY libc object ${file##*/}.o"
                valid_elf "musl/native/${file##*/}.o" || return 1
            done
            echo 'VERIFY libc repair archive and runtime outputs'
            valid_elf musl/native/native_syscall.o &&
                archive_has_repair_objects musl/native/librem-runtime.a &&
                [[ -s musl/native/libc.a ]]
            ;;
        compiler-selfbuild)
            valid_compiler native/chibicc && valid_compiler native/chibicc-selfbuilt &&
                compiler_source_valid && compiler_provenance_valid
            ;;
    esac
}

run_stage() {
    local name=$1 marker="$KIT/.$1.done" progress="$KIT/.$1.inprogress"
    shift
    if [[ -e $marker ]]; then
        echo "VERIFY $name checkpoint outputs"
        if stage_valid "$name"; then
            echo "SKIP $name (already complete)"
            return 0
        fi
        echo "RESUME $name (stale checkpoint or missing output; rebuilding stage)"
        rm -f -- "$marker"
    elif [[ -e $progress ]]; then
        echo "RESUME $name (previous command was interrupted; checking outputs is not enough to skip)"
        rm -f -- "$progress"
    else
        echo "VERIFY $name existing outputs"
        if stage_valid "$name"; then
            echo "SKIP $name (existing outputs verified; recording checkpoint)"
            : > "$marker"
            return 0
        fi
    fi
    echo "REM_STAGE|$name|start"
    : > "$progress"
    ( "$@" )
    stage_valid "$name" || { echo "FAIL $name: expected output missing" >&2; return 1; }
    : > "$marker"
    rm -f -- "$progress"
    echo "PASS $name"
    echo "REM_STAGE|$name|finish"
}

do_integer() {
    mkdir -p native
    cd "$KIT/bootstrap/chibicc"
    CC=${CC:-chibicc} REM_BOOTSTRAP_FLOAT=0 BOOTSTRAP_FLAGS=-DREM_INTEGER_BOOTSTRAP bash build-native.sh
    cp chibicc-native ../native/chibicc-integer
    mkdir -p ../native/include
    cp include/* ../native/include/
}

do_float() {
    cd "$KIT/bootstrap/compiler-rt-15.0.7.src"
    CC="$KIT/bootstrap/native/chibicc-integer" bash build-native.sh
    export CHIBICC_REM_SOFTFLOAT="$KIT/bootstrap/compiler-rt-15.0.7.src/native/ieee-runtime.o"
    cd "$KIT/bootstrap/chibicc"
    CC="$KIT/bootstrap/native/chibicc-integer" REM_BOOTSTRAP_FLOAT=1 \
        BOOTSTRAP_FLAGS=-DREM_BOOTSTRAP_FLOAT bash build-native.sh
    cp chibicc-native ../native/chibicc-float
}

do_decimal() {
    cd "$KIT/bootstrap/musl"
    CC="$KIT/bootstrap/native/chibicc-float" bash build-native.sh seed
    export CHIBICC_REM_LIBC="$KIT/bootstrap/musl/native/libc.a"
    cd "$KIT/bootstrap/chibicc"
    CC="$KIT/bootstrap/native/chibicc-float" REM_BOOTSTRAP_FLOAT=0 \
        BOOTSTRAP_FLAGS= bash build-native.sh
    cp chibicc-native ../native/chibicc-decimal
}

do_libc_repair() {
    cd "$KIT/bootstrap/musl"
    CC="$KIT/bootstrap/native/chibicc-decimal" bash build-native.sh full
}

do_compiler_selfbuild() {
    compiler_source_valid || {
        echo "FAIL compiler-selfbuild: bootstrap/chibicc/codegen.c is not the qualified aggregate-return source" >&2
        return 1
    }
    cd "$KIT/bootstrap/chibicc"
    CHIBICC_REM_LIBC="$KIT/bootstrap/musl/native/libc.a" \
        CHIBICC_REM_SOFTFLOAT="$KIT/bootstrap/compiler-rt-15.0.7.src/native/ieee-runtime.o" \
        REM_REBUILD_OBJECTS="codegen parse" \
        CC="$KIT/bootstrap/native/chibicc-decimal" REM_BOOTSTRAP_FLOAT=0 \
        BOOTSTRAP_FLAGS= bash build-native.sh
    cp chibicc-native ../native/chibicc
    CHIBICC_REM_LIBC="$KIT/bootstrap/musl/native/libc.a" \
        CHIBICC_REM_SOFTFLOAT="$KIT/bootstrap/compiler-rt-15.0.7.src/native/ieee-runtime.o" \
        REM_REBUILD_OBJECTS="codegen parse" \
        CC="$KIT/bootstrap/native/chibicc" REM_BOOTSTRAP_FLOAT=0 \
        BOOTSTRAP_FLAGS= bash build-native.sh
    cp chibicc-native ../native/chibicc-selfbuilt
    aggregate_abi_smoke ../native/chibicc-selfbuilt || {
        echo "FAIL compiler-selfbuild: rebuilt compiler failed aggregate return smoke test" >&2
        return 1
    }
    {
        printf 'source_sha256=%s\n' "$FIXED_CODEGEN_SHA256"
        printf 'parse_sha256=%s\n' "$FIXED_PARSE_SHA256"
        printf 'compiler_sha256=%s\n' "$(sha256sum ../native/chibicc-selfbuilt | cut -d' ' -f1)"
    } > ../native/chibicc-selfbuilt.provenance
    rm -f -- "$KIT/.compiler-porting.done" "$KIT/.compiler-porting.compiler.sha256"
    echo "COMPILER_PROVENANCE|binary=$KIT/bootstrap/native/chibicc-selfbuilt|$(tr '\n' '|' < ../native/chibicc-selfbuilt.provenance)"
    echo "PASS compiler-selfbuild aggregate return smoke (codegen $FIXED_CODEGEN_SHA256 parse $FIXED_PARSE_SHA256)"
}

run_stage bootstrap-integer do_integer
run_stage bootstrap-float do_float
run_stage bootstrap-decimal do_decimal
run_stage libc-repair do_libc_repair
run_stage compiler-selfbuild do_compiler_selfbuild
