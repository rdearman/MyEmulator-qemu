#!/bin/bash
set -eu
workspace=${REM_NATIVE_WORK:-/tmp/rem-native-userland-work}
tools=$(cd "$(dirname "$0")" && pwd)
name=$1
shift
mkdir -p "$workspace/host-build-plans/$name"
cd "$workspace/host-build-plans/$name"
export CC="$tools/chibicc-configure-cc"
export gl_cv_func_signbit_builtins=no
export ac_cv_c_stack_direction=-1
export sv_cv_stack_direction=-1
export sv_cv_stack_direction_msg="grows down"
bash "$workspace/kit/packages/$name/configure" \
    --build=x86_64-pc-linux-gnu --host=myemulator2-unknown-linux-musl \
    --prefix=/home/dev/.local CFLAGS=-O0 "$@" > configure.log 2>&1
make -s --eval='rem-built-sources:;@echo $(BUILT_SOURCES)' \
    rem-built-sources > built-sources.list
built_sources=$(cat built-sources.list)
if [ -n "$built_sources" ]; then
    make -j1 $built_sources > generated-sources.log 2>&1
fi
for directory in lib gnu gl/lib src; do
    if [ ! -f "$directory/Makefile" ]; then continue; fi
    make -s -C "$directory" --eval='rem-built-sources:;@echo $(BUILT_SOURCES)' \
        rem-built-sources > "$directory/built-sources.list"
    built_sources=$(cat "$directory/built-sources.list")
    if [ -n "$built_sources" ]; then
        make -j1 -C "$directory" $built_sources \
            > "$directory/generated-sources.log" 2>&1
    fi
done
echo "Configured source-only REM plan input: $name"
