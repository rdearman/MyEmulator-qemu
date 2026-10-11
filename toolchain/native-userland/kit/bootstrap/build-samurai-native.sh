#!/bin/bash
# Build Samurai with the qualified native chibicc and install it as
# bootstrap/tools/native/samu (on PATH via native-env.sh), then exercise it.
set -eu
case $0 in */*) cd "${0%/*}/.." ;; esac
source ./native-env.sh
src="$ROOT/packages/samurai"
test -f "$src/samu.c" || { echo "Samurai source missing: $src" >&2; exit 1; }
cd "$src"
objects=
for name in build deps env graph htab log os-posix parse samu scan tool tree util; do
    echo "CC samurai $name.c"
    "$CC" -DNO_POSIX_SPAWN -DCHIBICC_REM -I. -c -o "$name.o" "$name.c"
    objects="$objects $name.o"
done
ld -r -T "$ROOT/bootstrap/rem-reloc.ld" -o samu-partial.o $objects
"$CC" -o samu samu-partial.o
mkdir -p "$ROOT/bootstrap/tools/native"
install_dir="$ROOT/bootstrap/tools/native"
install_tmp="$install_dir/samu.new"
install_bin="$install_dir/samu"
# native-env.sh puts this directory on PATH, and an earlier bootstrap stage
# may leave a no-op `cp` placeholder there. Use the system BusyBox applet
# explicitly, preserve the linked binary's mode, and stage beside the target
# so the final rename is atomic on this filesystem.
/bin/busybox rm -f "$install_tmp"
/bin/busybox cp samu "$install_tmp"
test -s "$install_tmp" || {
    echo "Samurai install staging copy is missing or empty: $install_tmp" >&2
    exit 1
}
src_sha_line=$(sha256sum samu)
tmp_sha_line=$(sha256sum "$install_tmp")
src_sha=${src_sha_line%% *}
tmp_sha=${tmp_sha_line%% *}
test -n "$src_sha" && test "$src_sha" = "$tmp_sha" || {
    /bin/busybox rm -f "$install_tmp"
    echo "Samurai install staging hash mismatch" >&2
    exit 1
}
/bin/mv -f "$install_tmp" "$install_bin"
test -s "$install_bin" || {
    echo "Samurai install target is missing or empty: $install_bin" >&2
    exit 1
}
installed_sha_line=$(sha256sum "$install_bin")
installed_sha=${installed_sha_line%% *}
test "$installed_sha" = "$src_sha" || {
    echo "Samurai installed hash mismatch: $install_bin" >&2
    exit 1
}
echo "SAMURAI_SELECTED_SHA256|$installed_sha"
echo SAMURAI_BUILD_OK
samu="$install_bin"
run="$src/rem-selftest"
rm -rf "$run"
mkdir -p "$run"
cd "$run"
printf 'int main(void) { return 42; }\n' > hello.c
cat > build.ninja <<NINJA
rule cc
  command = $CC -o \$out \$in
build hello: cc hello.c
default hello
NINJA
"$samu" -n
test ! -e hello
echo SAMURAI_DRY_RUN_OK
"$samu"
if ./hello; then rc=0; else rc=$?; fi
test "$rc" -eq 42
echo SAMURAI_NATIVE_BUILD_OK
sleep 1
printf 'int main(void) { return 43; }\n' > hello.c
"$samu"
if ./hello; then rc=0; else rc=$?; fi
test "$rc" -eq 43
echo SAMURAI_INCREMENTAL_REBUILD_OK
"$samu" -t clean
test ! -e hello
echo SAMURAI_CLEAN_OK
"$samu"
if ./hello; then rc=0; else rc=$?; fi
test "$rc" -eq 43
echo SAMURAI_NATIVE_EXECUTION_OK
