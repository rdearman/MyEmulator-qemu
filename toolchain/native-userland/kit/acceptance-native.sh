#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}" ;; esac
source ./native-env.sh
mkdir -p native-tests
for file in bitfield-neighbours bool-bitfield brace-string-initializer \
    hex-literal-width printf-signed-width \
    printf-hex-width local-alignment assignment-call-alignment ieee-soft-float \
    libc-float-printf libc-strstr-frame dns-tcp; do
    "$CC" "tests/$file.c" -o "native-tests/$file"
    "native-tests/$file"
done
core="$ROOT/packages/coreutils-9.7/native/bin"
work="$ROOT/native-tests/operations-$$"
"$core/mkdir" "$work"
printf 'native operations\n' > "$work/a"
"$core/cp" "$work/a" "$work/b"
"$core/mv" "$work/b" "$work/c"
"$core/ln" "$work/c" "$work/d"
"$core/chmod" 600 "$work/d"
test "$("$core/cat" "$work/d")" = "native operations"
"$core/ls" "$work"
"$core/rm" "$work/a" "$work/c" "$work/d"
"$core/rm" -d "$work"
mkdir -p "$work"
printf 'alpha\nbeta\n' > "$work/input"
test "$(packages/findutils-4.10.0/native/bin/find "$work" -name input)" = "$work/input"
packages/grep-3.11/native/bin/grep '^beta$' "$work/input"
packages/sed-4.9/native/bin/sed 's/beta/gamma/' "$work/input" > "$work/edited"
packages/grep-3.11/native/bin/grep '^gamma$' "$work/edited"
packages/gawk-5.3.1/native/bin/gawk 'END { if (NR != 2) exit 1 }' "$work/input"
packages/diffutils-3.10/native/bin/cmp "$work/input" "$work/input"
packages/diffutils-3.10/native/bin/diff "$work/input" "$work/input"
TERMINFO="$ROOT/native-deps/share/terminfo" \
    packages/vim-9.1.0000/native/bin/vim -Nu NONE -n -es "$work/input" \
    '+%s/beta/vim-native/' '+wq'
packages/grep-3.11/native/bin/grep '^vim-native$' "$work/input"
packages/tar-1.35/native/bin/tar -cf "$work/files.tar" -C "$work" input
mkdir "$work/extracted"
packages/tar-1.35/native/bin/tar -xf "$work/files.tar" -C "$work/extracted"
packages/diffutils-3.10/native/bin/cmp "$work/input" "$work/extracted/input"
for compressor in gzip-1.13/gzip bzip2-1.0.8/bzip2 xz-5.6.3/xz; do
    package=${compressor%/*}
    program=${compressor#*/}
    "packages/$package/native/bin/$program" -c "$work/input" > "$work/compressed"
    "packages/$package/native/bin/$program" -dc "$work/compressed" > "$work/restored"
    packages/diffutils-3.10/native/bin/cmp "$work/input" "$work/restored"
done
"$core/rm" "$work/input" "$work/edited" "$work/files.tar" \
    "$work/compressed" "$work/restored" "$work/extracted/input"
"$core/rm" -d "$work/extracted" "$work"
curl="$ROOT/packages/curl-8.12.1/native/bin/curl"
"$curl" --fail --location --cacert "$ROOT/certificates/ca-bundle.crt" \
    https://example.com/ -o "$ROOT/native-tests/https-example.html"
grep 'Example Domain' "$ROOT/native-tests/https-example.html"
if [ -n "${SSH_TEST_HOST:-}" ]; then
    bash "$ROOT/tests/ssh-scp-acceptance.sh"
else
    echo "SSH_SCP_ACCEPTANCE_NOT_RUN: provide SSH_TEST_HOST/PORT/USER/KEY/KNOWN_HOSTS/SOURCE"
fi
echo "NATIVE_USERLAND_ACCEPTANCE_OK"
