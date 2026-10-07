#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}/.." ;; esac
source ./native-env.sh
package=$1
cd "$package"
test -f rem-native-plan/source-files.list
mkdir -p native-build native/bin
while IFS= read -r file; do
    directory=${file%/*}
    if [ "$directory" = "$file" ]; then directory=.; fi
    mkdir -p "native-build/$directory"
    sed "s|@REM_PREFIX@|$REM_PREFIX|g; s|@REM_DEPS@|$ROOT/native-deps|g" "rem-native-plan/$file" \
        > "native-build/$file"
done < rem-native-plan/source-files.list
sed "s|@CC@|$CC|g; s|@REM_DEPS@|$ROOT/native-deps|g; s|@REM_PREFIX@|$REM_PREFIX|g" \
    rem-native-plan/build.ninja.in > native-build/build.ninja
cd native-build
samu -j1 -f build.ninja
