#!/bin/sh
# REM Fold incremental update 20261008. Run in Termux with REM/QEMU STOPPED.
#   sh install.sh install  [IMAGE]   default IMAGE=$HOME/rem/rootfs.ext4
#   sh install.sh verify   [IMAGE]
#   sh install.sh rollback [IMAGE]
#   REM_KERNEL=/path/to/vmlinux overrides the kernel location (default: vmlinux beside IMAGE)
# Never resizes, formats or replaces the image. A guest file is replaced only if
# its current SHA-256 is the known baseline (or, for the two compiler sources, if
# the ABI diff applies cleanly to a locally modified copy). Every replaced file
# and the old kernel are backed up first; rollback restores them.
set -eu
ACTION=${1:-install}
IMG=${2:-$HOME/rem/rootfs.ext4}
HERE=$(cd "$(dirname "$0")" && pwd)
KDIR=$(cd "$(dirname "$IMG")" && pwd)
KERNEL=${REM_KERNEL:-$KDIR/vmlinux}
BK=${REM_UPDATE_BACKUP:-$KDIR/update-20261008-v2-backup}
LOG=$KDIR/update-20261008-v2.log
TMPD=${TMPDIR:-$HOME/.cache}/rem-update-20261008.$$
mkdir -p "$TMPD"
trap 'rm -rf "$TMPD"' EXIT
say() { echo "$*"; echo "$*" >> "$LOG"; }
die() { say "ERROR: $*"; exit 1; }
dbg() { debugfs -R "$1" "$IMG" 2>/dev/null; }
sha() { sha256sum "$1" | cut -d' ' -f1; }

for t in debugfs e2fsck sha256sum sed grep cut cmp; do
    command -v $t >/dev/null 2>&1 || die "missing $t (Termux: pkg install e2fsprogs)"
done
[ -f "$IMG" ] || die "image not found: $IMG"
case "$IMG" in *remvm*) die "refusing obsolete remvm image: $IMG" ;; esac
SIZE=$(wc -c < "$IMG" | tr -d ' ')
[ "$SIZE" -ge 9000000000 ] || die "image is $SIZE bytes; expected the 10 GiB ~/rem/rootfs.ext4"
IMGBASE=$(basename "$IMG")
for psargs in "-A -o args" "-ef" ""; do
    if ps $psargs 2>/dev/null | grep -v grep | grep qemu-system | grep -q -F "$IMGBASE"; then
        die "QEMU is running with $IMGBASE; stop REM first"
    fi
done
( cd "$HERE" && sha256sum -c payload.sha256 >/dev/null 2>&1 ) || die "package payload checksum mismatch (re-download)"

fsck_pre() {
    say "== e2fsck (preen) on $IMG =="
    set +e; e2fsck -fp "$IMG" >> "$LOG" 2>&1; rc=$?; set -e
    [ "$rc" -le 1 ] || die "e2fsck -p returned $rc; nothing changed. Run: e2fsck -f $IMG"
}
fsck_post() {
    set +e; e2fsck -fn "$IMG" >> "$LOG" 2>&1; rc=$?; set -e
    if [ "$rc" -eq 0 ]; then say "PASS filesystem check"; else die "e2fsck -fn rc=$rc (see $LOG)"; fi
}
gsha() { # SHA-256 of a guest file, or ABSENT
    rm -f "$TMPD/g"; dbg "dump -p \"$1\" \"$TMPD/g\"" >/dev/null
    if [ -f "$TMPD/g" ]; then sha "$TMPD/g"; else echo ABSENT; fi
}
gexists() { dbg "stat \"$1\"" | grep -q 'Inode:'; }
gattr() { # octal-perm uid gid
    dbg "stat \"$1\"" > "$TMPD/st"
    m=$(sed -n 's/.*Mode: *\(0[0-7]*\).*/\1/p' "$TMPD/st" | head -n 1)
    u=$(sed -n 's/.*User: *\([0-9]*\).*/\1/p' "$TMPD/st" | head -n 1)
    g=$(sed -n 's/.*Group: *\([0-9]*\).*/\1/p' "$TMPD/st" | head -n 1)
    echo "${m:-0644} ${u:-0} ${g:-0}"
}
gput() { # local guestpath perm uid gid
    {
        gexists "$2" && echo "rm \"$2\""
        echo "write \"$1\" \"$2\""
        echo "sif \"$2\" mode 0$(printf %o $((0100000 | (0$3 & 07777))))"
        echo "sif \"$2\" uid $4"
        echo "sif \"$2\" gid $5"
    } > "$TMPD/cmd"
    debugfs -w -f "$TMPD/cmd" "$IMG" >> "$LOG" 2>&1
    [ "$(gsha "$2")" = "$(sha "$1")" ] || die "write verification failed for $2"
}
gmkdir() { # create guest directory and missing parents; owner inherited from parent
    [ -z "$1" ] && return 0
    gexists "$1" && return 0
    gmkdir "$(dirname "$1")"
    set -- "$1" $(gattr "$(dirname "$1")")
    debugfs -w -R "mkdir \"$1\"" "$IMG" >> "$LOG" 2>&1
    debugfs -w -R "sif \"$1\" uid $3" "$IMG" >> "$LOG" 2>&1
    debugfs -w -R "sif \"$1\" gid $4" "$IMG" >> "$LOG" 2>&1
    echo "$1" >> "$BK/dirs.created"; say "MKDIR $1"
}

match_fragment() { # exact, unique whole-fragment match; no patch/awk dependency
    mf_lines=$(wc -l < "$2")
    [ "$mf_lines" -gt 0 ] || die "empty compiler patch context"
    if grep -n -F -x -e "$(sed -n '1p' "$2")" "$1" > "$TMPD/positions"; then
        :
    else
        mf_rc=$?
        [ "$mf_rc" = 1 ] || die "cannot search compiler patch context"
    fi
    mf_count=0
    mf_line=0
    while IFS=: read -r number text; do
        sed -n "${number},$((number + mf_lines - 1))p" "$1" > "$TMPD/fragment"
        if cmp -s "$TMPD/fragment" "$2"; then
            mf_count=$((mf_count + 1))
            mf_line=$number
        fi
    done < "$TMPD/positions"
    [ "$mf_count" -le 1 ] || die "ambiguous compiler patch context; no guest files changed"
}

patch_hunk() {
    : > "$TMPD/oldpart"
    : > "$TMPD/newpart"
    sed -n '4,$p' "$2" > "$TMPD/hunkbody"
    while IFS= read -r hline; do
        case "$hline" in
            ' '*)
                printf '%s\n' "${hline#?}" >> "$TMPD/oldpart"
                printf '%s\n' "${hline#?}" >> "$TMPD/newpart"
                ;;
            '-'*) printf '%s\n' "${hline#?}" >> "$TMPD/oldpart" ;;
            '+'*) printf '%s\n' "${hline#?}" >> "$TMPD/newpart" ;;
            *) die "unsupported compiler patch line" ;;
        esac
    done < "$TMPD/hunkbody"
    match_fragment "$1" "$TMPD/oldpart"
    if [ "$mf_count" = 1 ]; then
        {
            [ "$mf_line" -le 1 ] || sed -n "1,$((mf_line - 1))p" "$1"
            cat "$TMPD/newpart"
            sed -n "$((mf_line + mf_lines)),\$p" "$1"
        } > "$TMPD/merged"
        mv "$TMPD/merged" "$1"
    else
        match_fragment "$1" "$TMPD/newpart"
        [ "$mf_count" = 1 ] ||
            die "compiler source conflicts with update; no guest files changed (see $LOG)"
        say "NOTE compiler hunk already applied; keeping local changes"
    fi
}

apply_source_diff() { # local-source diff; accept each already-applied hunk
    as_target=$1
    sed -n '1,2p' "$2" > "$TMPD/header"
    sed -n '3,$p' "$2" > "$TMPD/body"
    rm -f "$TMPD/hunk"
    while IFS= read -r line; do
        case "$line" in
            '@@ '*)
                [ ! -f "$TMPD/hunk" ] || patch_hunk "$as_target" "$TMPD/hunk"
                cp "$TMPD/header" "$TMPD/hunk"
                ;;
        esac
        printf '%s\n' "$line" >> "$TMPD/hunk"
    done < "$TMPD/body"
    [ ! -f "$TMPD/hunk" ] || patch_hunk "$as_target" "$TMPD/hunk"
}

prepare_compiler_sources() {
    while IFS='|' read -r gp pl base new pt nm; do
        [ "$pt" = 1 ] || continue
        cur=$(gsha "$gp")
        [ "$cur" != ABSENT ] || die "compiler source missing: $gp"
        if [ "$cur" = "$new" ] || [ "$cur" = "$base" ]; then
            cp "$HERE/payload/$pl" "$TMPD/prepared-$pl"
        else
            cp "$TMPD/g" "$TMPD/prepared-$pl"
            apply_source_diff "$TMPD/prepared-$pl" "$HERE/payload/$pl.diff"
        fi
    done < "$HERE/files.list"
}

supported_previous_file() {
    case "${3:-}:$2" in
        /home/dev/rem-native-userland/bootstrap/musl/native/libc.a:0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186|\
        /home/dev/rem-update-20261008/verify.sh:5e93a389256a2c05f11dc26930518215f727511efc26010e7205cbc59ead1cf5)
            return 0 ;;
    esac
    case "$1:$2" in
        chibicc-rem-native.patch:ad77b8b945638eb6287a582165725ef5655b12fa606052f5567468ea26561c04|\
        rebuild-required-native.sh:9e5386300f930ccb42b949bd10838d94975c5e09c38325b42aa8037a4112542d|\
        compiler-porting-native.sh:5c47f7be9b69d575bf5edb0d30283967b456ed242cd9385593de7559b02a6a16|\
        libc.a:30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924)
            return 0 ;;
        *) return 1 ;;
    esac
}

preflight_files() {
    while IFS='|' read -r gp pl base new pt nm; do
        [ -n "$gp" ] || continue
        [ "$pt" = 1 ] && continue
        cur=$(gsha "$gp")
        [ "$cur" = "$new" ] && continue
        [ "$base" = NEW ] && [ "$cur" = ABSENT ] && continue
        [ "$cur" = "$base" ] && continue
        supported_previous_file "$pl" "$cur" "$gp" && continue
        die "unsupported or missing baseline for $gp ($cur); no guest or kernel files changed"
    done < "$HERE/files.list"
}

do_install() {
    [ -e "$BK/installed" ] && say "NOTE: earlier install recorded in $BK; re-running is safe"
    [ -f "$KERNEL" ] || die "kernel missing: $KERNEL (set REM_KERNEL to its real path)"
    # Both halves of the ABI change must merge before changing either file.
    prepare_compiler_sources
    preflight_files
    fsck_pre
    mkdir -p "$BK/files"
    # Qualification replaces these later, inside REM; rollback must restore
    # the compiler and Samurai from before this update as well as the sources.
    for gp in /home/dev/rem-native-userland/bootstrap/native/chibicc-selfbuilt \
        /home/dev/rem-native-userland/bootstrap/tools/native/samu; do
        key=$(echo "$gp" | sed 's#/#_#g')
        if [ -f "$BK/files.created" ] && grep -Fqx "$gp" "$BK/files.created"; then
            continue
        fi
        if [ "$(gsha "$gp")" != ABSENT ]; then
            if [ ! -f "$BK/files/$key" ]; then
                cp "$TMPD/g" "$BK/files/$key"
                echo "$gp|$key|$(gattr "$gp")" >> "$BK/restore.list"
            fi
        else
            grep -q "^$gp$" "$BK/files.created" 2>/dev/null ||
                echo "$gp" >> "$BK/files.created"
        fi
    done
    # Kernel (outside the image)
    if [ -f "$KERNEL" ]; then
        kcur=$(sha "$KERNEL"); knew=$(sha "$HERE/payload/vmlinux")
        if [ "$kcur" = "$knew" ]; then say "SKIP (already updated) $KERNEL"
        else
            [ "$kcur" = 576c1f4fc599e02554e625463d1cdc0455c3e99caff4f047c06fe3c2e537d619 ] ||
                say "NOTE $KERNEL is not the expected R4 kernel ($kcur); backing it up anyway"
            [ -f "$BK/vmlinux.orig" ] || cp -p "$KERNEL" "$BK/vmlinux.orig"
            cp "$HERE/payload/vmlinux" "$KERNEL.new" && mv "$KERNEL.new" "$KERNEL"
            [ "$(sha "$KERNEL")" = "$knew" ] || die "kernel copy failed"
            say "UPDATED $KERNEL (old copy: $BK/vmlinux.orig)"
        fi
    fi
    while read -r d; do [ -n "$d" ] && gmkdir "$d"; done < "$HERE/dirs.list"
    # files.list: guestpath|payload|baseline-sha(or NEW)|new-sha|patchable|mode-for-new
    while IFS='|' read -r gp pl base new pt nm; do
        [ -n "$gp" ] || continue
        cur=$(gsha "$gp")
        key=$(echo "$gp" | sed 's#/#_#g')
        src="$HERE/payload/$pl"
        if [ "$cur" = "$new" ]; then say "SKIP (already updated) $gp"; continue; fi
        if [ "$base" = NEW ]; then
            if [ "$cur" != ABSENT ]; then
                [ -f "$BK/files/$key" ] || cp "$TMPD/g" "$BK/files/$key"
                grep -q "^$gp|" "$BK/restore.list" 2>/dev/null || echo "$gp|$key|$(gattr "$gp")" >> "$BK/restore.list"
                set -- $(gattr "$gp")
            else
                gmkdir "$(dirname "$gp")"
                set -- $nm $(gattr "$(dirname "$gp")" | cut -d' ' -f2-)
                echo "$gp" >> "$BK/files.created"
            fi
            gput "$src" "$gp" "$1" "$2" "$3"; say "ADDED $gp"; continue
        fi
        [ "$cur" != ABSENT ] || die "$gp disappeared after preflight; rollback required"
        if [ "$pt" = 1 ]; then
            src="$TMPD/prepared-$pl"
            if [ "$(sha "$src")" = "$cur" ]; then
                say "SKIP (already merged; local changes preserved) $gp"; continue
            fi
        elif [ "$cur" != "$base" ]; then
            supported_previous_file "$pl" "$cur" "$gp" ||
                die "$gp changed after preflight; rollback required"
        fi
        if [ ! -f "$BK/files/$key" ]; then
            cp "$TMPD/g" "$BK/files/$key"; echo "$gp|$key|$(gattr "$gp")" >> "$BK/restore.list"
        fi
        set -- $(gattr "$gp")
        gput "$src" "$gp" "$1" "$2" "$3"
        say "UPDATED $gp"
    done < "$HERE/files.list"
    # Missing BusyBox applet links only; existing files/links are never touched.
    for p in $(cat "$HERE/applets.list"); do
        gexists "$p" && continue
        gexists "$(dirname "$p")" || continue
        debugfs -w -R "symlink $p /bin/busybox" "$IMG" >> "$LOG" 2>&1
        echo "$p" >> "$BK/links.created"; say "LINK $p -> /bin/busybox"
    done
    do_verify
    date > "$BK/installed"
    say "INSTALL COMPLETE. Backups: $BK  Log: $LOG"
    say "Start REM, log in, run:  sh /home/dev/rem-update-20261008/verify.sh full"
}

do_verify() {
    fails=0
    if [ -f "$KERNEL" ] && [ "$(sha "$KERNEL")" = "$(sha "$HERE/payload/vmlinux")" ]; then say "PASS $KERNEL"; else say "NOT-UPDATED $KERNEL"; fails=1; fi
    while IFS='|' read -r gp pl base new pt nm; do
        [ -n "$gp" ] || continue
        cur=$(gsha "$gp")
        if [ "$cur" = "$new" ]; then say "PASS $gp"
        elif [ "$cur" = "$base" ] || [ "$cur" = ABSENT ]; then say "NOT-UPDATED $gp"; fails=1
        elif [ "$pt" = 1 ]; then
            cp "$TMPD/g" "$TMPD/verify-$pl"
            apply_source_diff "$TMPD/verify-$pl" "$HERE/payload/$pl.diff"
            if cmp -s "$TMPD/g" "$TMPD/verify-$pl"; then
                say "PASS (merged local compiler changes) $gp"
            else
                say "NOT-UPDATED $gp"; fails=1
            fi
        else say "NOT-UPDATED (local file preserved) $gp ($cur)"; fails=1; fi
    done < "$HERE/files.list"
    fsck_post
    if [ "$fails" = 0 ]; then say "VERIFY_OK"; else say "VERIFY_INCOMPLETE"; return 1; fi
}

do_rollback() {
    [ -d "$BK" ] || die "no backup found in $BK"
    fsck_pre
    if [ -f "$BK/links.created" ]; then
        for p in $(cat "$BK/links.created"); do debugfs -w -R "rm $p" "$IMG" >> "$LOG" 2>&1 || true; say "UNLINK $p"; done
        mv "$BK/links.created" "$BK/links.removed"
    fi
    if [ -f "$BK/files.created" ]; then
        for p in $(cat "$BK/files.created"); do debugfs -w -R "rm \"$p\"" "$IMG" >> "$LOG" 2>&1 || true; say "REMOVED $p"; done
        mv "$BK/files.created" "$BK/files.removed"
    fi
    if [ -f "$BK/restore.list" ]; then
        while IFS='|' read -r gp key attrs; do
            set -- $attrs
            gput "$BK/files/$key" "$gp" "$1" "$2" "$3"; say "RESTORED $gp"
        done < "$BK/restore.list"
        mv "$BK/restore.list" "$BK/restore.done"
    fi
    if [ -f "$BK/dirs.created" ]; then
        sed -n '1!G;h;$p' "$BK/dirs.created" > "$TMPD/dirs"
        while read -r d; do
            [ "$d" = /home/dev/rem-update-20261008 ] && debugfs -w -R "rm $d/verify.sh" "$IMG" >> "$LOG" 2>&1
            debugfs -w -R "rmdir \"$d\"" "$IMG" >> "$LOG" 2>&1 && say "RMDIR $d" || say "KEPT $d (not empty)"
        done < "$TMPD/dirs"
        mv "$BK/dirs.created" "$BK/dirs.removed"
    fi
    if [ -f "$BK/vmlinux.orig" ]; then
        cp "$BK/vmlinux.orig" "$KERNEL.new" && mv "$KERNEL.new" "$KERNEL"; say "RESTORED $KERNEL"
    fi
    rm -f "$BK/installed"
    fsck_post
    old="$BK.rolledback-$(date +%Y%m%d%H%M%S)"
    mv "$BK" "$old"
    say "ROLLBACK COMPLETE (old backups kept in $old)"
}

say "== REM update 20261008 v2: $ACTION on $IMG ($(date)) =="
case "$ACTION" in
    install) do_install ;;
    verify) do_verify ;;
    rollback) do_rollback ;;
    *) die "usage: sh install.sh {install|verify|rollback} [IMAGE]" ;;
esac
