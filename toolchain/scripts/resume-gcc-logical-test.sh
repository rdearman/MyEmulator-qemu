#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/rick/Development/Active/MyEmulator-qemu
BUILD=/tmp/myemu-gcc2/build
PREFIX="$ROOT/.toolchain-install"
GCC="$PREFIX/bin/myemulator2-elf-gcc"
INSTALLED_CC1="$PREFIX/libexec/gcc/myemulator2-elf/15.2.0/cc1"
QEMU="$ROOT/.qemu-build/qemu-system-myemulator32"
LOG=/tmp/myemu-gcc2/gcc-resume-install.log
JOBS=${JOBS:-2}

cd "$ROOT"

exec > >(tee -a "$LOG") 2>&1

echo "Build log: $LOG"
echo "Build directory: $BUILD"
echo "Install prefix: $PREFIX"

test -f "$BUILD/Makefile"
test -f "$BUILD/gcc/Makefile"
test -x "$PREFIX/bin/myemulator2-elf-as"
test -x "$PREFIX/bin/myemulator2-elf-ld"

echo "== Resume GCC compiler build =="
if test -x "$BUILD/gcc/cc1" || test -x "$INSTALLED_CC1"; then
        echo "existing cc1 found; skipping completed all-gcc target"
else
        make -C "$BUILD" -j"$JOBS" all-gcc
fi

echo "== Verify compiler components =="
test -x "$BUILD/gcc/cc1" || test -x "$INSTALLED_CC1"
test -x "$BUILD/gcc/xgcc"

echo "== Install GCC only =="
if test -x "$GCC" && test -x "$INSTALLED_CC1"; then
        echo "installed compiler already present; skipping install-gcc"
else
        make -C "$BUILD" install-gcc
fi

test -x "$GCC"
"$GCC" --version | head -n 1

TEST_DIR=$(mktemp -d /tmp/myemu-gcc2/logical-immediate-test.XXXXXX)
trap 'rm -rf "$TEST_DIR"' EXIT

cat >"$TEST_DIR/logical.c" <<'EOF'
volatile unsigned inputs[4] = {
        0x00001000u,
        0x0000101fu,
        0xffffffffu,
        0x00000020u
};

volatile unsigned outputs[4];

__attribute__((noinline))
static unsigned align32(unsigned value)
{
        return (value + 31u) & ~31u;
}

int main(void)
{
        unsigned i;

        for (i = 0; i < 4; ++i)
                outputs[i] = align32(inputs[i]);

        if (outputs[0] != 0x00001000u)
                return 1;
        if (outputs[1] != 0x00001020u)
                return 2;
        if (outputs[2] != 0x00000000u)
                return 3;
        if (outputs[3] != 0x00000020u)
                return 4;

        return 0;
}
EOF

echo "== Verify corrected logical-immediate code generation =="

for OPT in -O0 -O1 -O2 -Os; do
        TAG=${OPT#-}
        ASM="$TEST_DIR/logical-$TAG.s"

        "$GCC" \
                -ffreestanding \
                -fno-builtin \
                -fno-asynchronous-unwind-tables \
                "$OPT" \
                -S "$TEST_DIR/logical.c" \
                -o "$ASM"

        echo "--- $OPT ---"
        grep -E '\b(andi|ori|xori)\b' "$ASM" || true

        if grep -Eq '\b(andi|ori|xori)\b[^#\n]*,[[:space:]]*-[0-9]+' "$ASM"; then
                echo "invalid signed logical immediate emitted at $OPT" >&2
                exit 1
        fi

        if grep -Eq '\b(andi|ori|xori)\b[^#\n]*,[[:space:]]*(4064|0x0?fe0)\b' "$ASM"; then
                echo "potentially invalid zero-extended alignment mask emitted at $OPT" >&2
                exit 1
        fi
done

echo "logical-immediate assembly checks: PASS"

echo "== Assemble and execute focused regression =="

for OPT in -O0 -O1 -O2 -Os; do
        TAG=${OPT#-}
        CRT="$TEST_DIR/crt0-$TAG.o"
        OBJ="$TEST_DIR/logical-$TAG.o"
        ELF="$TEST_DIR/logical-$TAG.elf"

        "$GCC" \
                -ffreestanding \
                -fno-builtin \
                -fno-asynchronous-unwind-tables \
                -nostdlib \
                -nostartfiles \
                -nodefaultlibs \
                -c "$ROOT/toolchain/examples/crt0.S" \
                -o "$CRT"

        "$GCC" \
                -ffreestanding \
                -fno-builtin \
                -fno-asynchronous-unwind-tables \
                -nostdlib \
                -nostartfiles \
                -nodefaultlibs \
                "$OPT" \
                -c "$TEST_DIR/logical.c" \
                -o "$OBJ"

        "$GCC" \
                -nostdlib \
                -nostartfiles \
                -nodefaultlibs \
                "$CRT" "$OBJ" \
                -o "$ELF"

        "$PREFIX/bin/myemulator2-elf-readelf" -h "$ELF" | \
                grep -E 'ELF32|MyEmulator2'

        python3 - "$QEMU" "$ELF" <<'PY'
import importlib.util
import sys
from pathlib import Path

qemu = sys.argv[1]
elf = Path(sys.argv[2])
test_file = Path.cwd() / "toolchain/tests/test-binutils.py"

spec = importlib.util.spec_from_file_location("myemu_binutils_tests", test_file)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

module.run_elf(qemu, elf, 0, "R1")
PY

        echo "$OPT: execution PASS"
done

echo "focused logical-immediate regression: PASS"
echo "Installed compiler: $GCC"
echo "Complete log: $LOG"
