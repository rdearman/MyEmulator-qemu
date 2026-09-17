#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/../.." && pwd)"
qemu_repo="${QEMU_REPO:-https://gitlab.com/qemu-project/qemu.git}"
qemu_ref="${QEMU_REF:-v9.2.0}"
qemu_source="${QEMU_SOURCE:-$project_root/../qemu}"
qemu_build="${QEMU_BUILD:-$project_root/../qemu-build-myemulator}"
jobs="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')}"
run_tests=0

usage()
{
    printf 'usage: %s [--test]\n' "$0"
    printf '\nEnvironment:\n'
    printf '  QEMU_SOURCE  full QEMU checkout (default: %s)\n' "$qemu_source"
    printf '  QEMU_BUILD   out-of-tree build directory (default: %s)\n' "$qemu_build"
    printf '  QEMU_REF     QEMU tag or branch to clone (default: %s)\n' "$qemu_ref"
    printf '  QEMU_REPO    QEMU Git URL (default: %s)\n' "$qemu_repo"
    printf '  JOBS         parallel build jobs (default: %s)\n' "$jobs"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --test)
            run_tests=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if [ ! -d "$qemu_source" ]; then
    printf 'Cloning QEMU %s at %s into %s\n' "$qemu_repo" "$qemu_ref" "$qemu_source"
    git clone --branch "$qemu_ref" --depth 1 "$qemu_repo" "$qemu_source"
elif [ ! -x "$qemu_source/configure" ]; then
    printf 'QEMU_SOURCE exists but is not a full QEMU checkout: %s\n' "$qemu_source" >&2
    exit 1
fi

printf 'Applying MyEmulator overlay to %s\n' "$qemu_source"
"$project_root/qemu/tools/apply-overlay.sh" "$qemu_source"

mkdir -p "$qemu_build"
printf 'Configuring %s\n' "$qemu_build"
(cd "$qemu_build" && "$qemu_source/configure" \
    --target-list=myemulator-softmmu \
    --disable-dbus-display \
    --disable-werror \
    --enable-debug-tcg)

printf 'Building qemu-system-myemulator with %s job(s)\n' "$jobs"
make -C "$qemu_build" -j"$jobs" qemu-system-myemulator

qemu_binary="$qemu_build/qemu-system-myemulator"
printf 'Built %s\n' "$qemu_binary"

if [ "$run_tests" -eq 1 ]; then
    printf 'Running MyEmulator CPU tests\n'
    QEMU_MYEMULATOR="$qemu_binary" "$project_root/tests/run-qemu-cpu-tests.sh"
fi
