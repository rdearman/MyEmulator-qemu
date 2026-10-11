#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}/.." ;; esac
source ./native-env.sh
compiler=${TEST_CC:-$CC}
mkdir -p native-tests/porting
run_caller_abi_tests() {
    as tests/aggregate-gcc-helpers.s -o native-tests/porting/gcc-helpers.o
    "$compiler" -DAGGREGATE_EXTERNAL -DAGGREGATE_SMALL_RETURN_ONLY \
        tests/aggregate-abi.c native-tests/porting/gcc-helpers.o \
        -o native-tests/porting/chibicc-small-return-caller
    if native-tests/porting/chibicc-small-return-caller; then
        echo "PASS chibicc caller small aggregate return / REM ABI"
    else
        rc=$?
        echo "FAIL chibicc caller small aggregate return (exit $rc)" >&2
        exit "$rc"
    fi
    "$compiler" -DAGGREGATE_EXTERNAL tests/aggregate-abi.c \
        native-tests/porting/gcc-helpers.o -o native-tests/porting/chibicc-caller
    if native-tests/porting/chibicc-caller; then
        echo "PASS chibicc caller / GCC aggregate callee"
    else
        rc=$?
        echo "FAIL chibicc-caller (exit $rc)" >&2
        exit "$rc"
    fi
    as tests/aggregate-gcc-caller.s -o native-tests/porting/gcc-caller.o
    "$compiler" -DAGGREGATE_NO_MAIN tests/aggregate-abi.c \
        native-tests/porting/gcc-caller.o -o native-tests/porting/gcc-caller
    if native-tests/porting/gcc-caller; then
        echo "PASS GCC caller / chibicc aggregate callee"
    else
        rc=$?
        echo "FAIL gcc-caller (exit $rc)" >&2
        exit "$rc"
    fi
}
if [[ ${REM_PORTING_START_AT:-} == chibicc-caller ]]; then
    run_caller_abi_tests
    echo "COMPILER_PORTING_RESUME|PASS"
    exit 0
fi
for name in aggregate-small-return aggregate-abi alloca-frame vla-frame long-conditional-branch designator-unnamed-bitfield \
    preprocessor-signed-comparison line-in-conditional \
    parenthesized-function-parameter nested-designator-continuation \
    static-assert; do
    "$compiler" "tests/$name.c" -o "native-tests/porting/$name"
    "native-tests/porting/$name"
    echo "PASS $name"
done
"$compiler" -Itests/include-next/first -Itests/include-next/second \
    tests/include-next/main.c -o native-tests/porting/include-next
native-tests/porting/include-next
echo "PASS include-next"
if "$compiler" tests/static-assert-fail.c -o native-tests/porting/rejected \
    > native-tests/porting/static-assert-fail.log 2>&1; then
    echo "FAIL static-assert-fail was accepted" >&2
    exit 1
fi
grep -q "assert" native-tests/porting/static-assert-fail.log
echo "PASS static-assert-fail rejection"
if "$compiler" tests/negative-array-bound.c -o native-tests/porting/rejected \
    > native-tests/porting/negative-array-bound.log 2>&1; then
    echo "FAIL negative array bound was accepted" >&2
    exit 1
fi
grep -q "negative array bound" native-tests/porting/negative-array-bound.log
echo "PASS negative array bound rejection"
if "$compiler" -Ibootstrap/chibicc/include tests/atomic-exchange.c \
    -o native-tests/porting/rejected \
    > native-tests/porting/atomic-exchange.log 2>&1; then
    echo "FAIL unsupported C11 atomics were accepted" >&2
    exit 1
fi
grep -q "C11 atomics are not implemented" native-tests/porting/atomic-exchange.log
echo "PASS unsupported atomic capability rejection"
"$compiler" -Ibootstrap/chibicc tests/hashmap-tombstone.c \
    bootstrap/chibicc/hashmap.c bootstrap/chibicc/strings.c \
    -o native-tests/porting/hashmap-tombstone
native-tests/porting/hashmap-tombstone
echo "PASS hashmap-tombstone"
run_caller_abi_tests
echo COMPILER_REQUIRED_PORTING_TESTS_OK
