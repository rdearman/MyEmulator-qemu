#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}" ;; esac
mkdir -p logs
trap 'result=$?; echo "Native build failed; inspect logs/ for the failing stage." >&2; exit "$result"' ERR
if test -f BUILD-ACCEPTED; then rm BUILD-ACCEPTED; fi
bash bootstrap/build-native.sh > logs/bootstrap.log 2>&1
source ./native-env.sh
bash tests/compiler-porting-native.sh > logs/compiler-porting.log 2>&1
bash bootstrap/tools/build-native.sh > logs/bootstrap-tools.log 2>&1
bash packages/bearssl-0.6/build-native.sh > logs/bearssl.log 2>&1
bash packages/curl-8.12.1/build-native.sh > logs/curl.log 2>&1
bash packages/coreutils-9.7/build-native.sh > logs/coreutils.log 2>&1
for package in dropbear-2024.86 findutils-4.10.0 grep-3.11 sed-4.9 tar-1.35 \
    gzip-1.13 bzip2-1.0.8 xz-5.6.3 diffutils-3.10 gawk-5.3.1 \
    ncurses-6.5 vim-9.1.0000; do
    bash "packages/$package/build-native.sh" > "logs/$package.log" 2>&1
done
bash acceptance-native.sh > logs/acceptance.log 2>&1
printf '%s\n' "$REM_PREFIX" > BUILD-ACCEPTED
echo "Native compiler/runtime and complete userland built and accepted."
