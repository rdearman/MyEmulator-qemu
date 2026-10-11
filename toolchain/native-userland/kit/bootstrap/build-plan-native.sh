#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}/.." ;; esac
source ./native-env.sh
package=$1
package_name=${package##*/}
plan_root=$ROOT/build-plans/$package_name
cd "$package"
test -f "$plan_root/source-files.list"
mkdir -p native-build native/bin
while IFS= read -r file; do
    directory=${file%/*}
    if [ "$directory" = "$file" ]; then directory=.; fi
    mkdir -p "native-build/$directory"
    rendered="native-build/$file"
    rm -f -- "$rendered.tmp."*
    sed "s|@REM_PREFIX@|$REM_PREFIX|g; s|@REM_DEPS@|$ROOT/native-deps|g" \
        "$plan_root/$file" > "$rendered.tmp.$$"
    if [ -f "$rendered" ] && cmp -s "$rendered.tmp.$$" "$rendered"; then
        rm -f -- "$rendered.tmp.$$"
    else
        mv -f -- "$rendered.tmp.$$" "$rendered"
    fi
done < rem-native-plan/source-files.list
rendered_ninja=native-build/build.ninja.rendered.tmp
atomic_ninja=native-build/build.ninja.tmp
sed "s|@CC@|$CC|g; s|@REM_DEPS@|$ROOT/native-deps|g; s|@REM_PREFIX@|$REM_PREFIX|g" \
    "$plan_root/build.ninja.in" > "$rendered_ninja"
: > "$atomic_ninja"
while IFS= read -r line || [[ -n $line ]]; do
    case $line in
        '  command = mkdir -p $directory && $cc $flags -c $in -o $out')
            printf '%s\n' '  command = mkdir -p $directory && rm -f $out.tmp && $cc $flags -c $in -o $out.tmp && mv -f $out.tmp $out' >> "$atomic_ninja" ;;
        '  command = ar rcs $out $in')
            printf '%s\n' '  command = rm -f $out.tmp && ar rcs $out.tmp $in && mv -f $out.tmp $out' >> "$atomic_ninja" ;;
        '  command = mkdir -p ../native/bin && ld -r -T $root/bootstrap/rem-reloc.ld -o $out.partial.o $in && $cc -o $out $out.partial.o $libraries')
            printf '%s\n' '  command = mkdir -p ../native/bin && ld -r -T $root/bootstrap/rem-reloc.ld -o $out.partial.o.tmp $in && mv -f $out.partial.o.tmp $out.partial.o && $cc -o $out.tmp $out.partial.o $libraries && mv -f $out.tmp $out' >> "$atomic_ninja" ;;
        *) printf '%s\n' "$line" >> "$atomic_ninja" ;;
    esac
done < "$rendered_ninja"
rm -f "$rendered_ninja"
mv -f "$atomic_ninja" native-build/build.ninja
cd native-build
echo "REM_STAGE|${package##*/}|samu-start"
samu -j1 -f build.ninja
echo "PASS ${package##*/} Samurai build"
