#!/bin/bash
set -eu
workspace=${REM_NATIVE_WORK:-/tmp/rem-native-userland-work}
tools=$(cd "$(dirname "$0")" && pwd)
source="$workspace/kit/packages/vim-9.1.0000/src"
mkdir -p "$workspace/host-build-plans/vim-9.1.0000/auto"
cd "$workspace/host-build-plans/vim-9.1.0000"
# These are explicit musl/ncurses contracts, not host runtime test results.
# Native libc and terminal acceptance must validate them before release.
export vim_cv_toupper_broken=no vim_cv_terminfo=yes
export vim_cv_tgetent=zero vim_cv_getcwd_broken=no
export vim_cv_stat_ignores_slash=no vim_cv_memmove_handles_overlap=yes
export CPPFLAGS="-I$workspace/host-build-plans/native-deps/include -I."
export LDFLAGS="-L$workspace/host-build-plans/native-deps/lib"
export CC="$tools/chibicc-configure-cc"
bash "$source/auto/configure" --srcdir="$source" \
    --build=x86_64-pc-linux-gnu --host=myemulator2-unknown-linux-musl \
    --prefix=/home/dev/.local --with-features=normal --with-tlib=ncurses \
    --disable-gui --without-x --disable-nls --disable-pythoninterp \
    --disable-python3interp --disable-perlinterp --disable-rubyinterp \
    --disable-luainterp --disable-tclinterp --disable-mzschemeinterp \
    --disable-netbeans --disable-terminal CFLAGS=-O0 > configure.log 2>&1
cp "$source/Makefile" Makefile
cp "$source/Make_all.mak" Make_all.mak
mkdir -p testdir
cp "$source/testdir/Make_all.mak" testdir/Make_all.mak
make srcdir="$source" VPATH="$source" COMPILEDBY=REM \
    VIMRUNTIMEDIR=/home/dev/.local/share/vim/vim91 \
    auto/pathdef.c auto/osdef.h > generated-sources.log 2>&1
echo VIM_SOURCE_CONFIGURATION_READY
