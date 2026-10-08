#!/bin/bash
# Qualify a disposable stopped-image copy using the exact actual Fold archive.
# Usage: bash qualify-fold-libc.sh IMAGE PACKAGE FOLD_LIBC NEW_WORK_DIRECTORY
set -euo pipefail
base=${1:?provide a stopped baseline image}
package=$(cd "${2:?provide the candidate package}" && pwd)
fold_libc=${3:?provide the actual Fold libc archive}
work=${4:?provide a new scratch directory}
here=$(cd "${0%/*}" && pwd)
repo=$(cd "$here/../.." && pwd)
qemu=${QEMU:-$repo/REM-FLIGHT-HUMAN-UAT-FULL/qemu-system-myemulator32}
expected=0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186
[ "$(sha256sum "$fold_libc" | cut -d' ' -f1)" = "$expected" ] ||
    { echo "actual Fold libc hash mismatch" >&2; exit 1; }
case "$work" in /tmp/*) ;; *) echo "qualification output must be disposable /tmp scratch" >&2; exit 1 ;; esac
[ ! -e "$work" ] || { echo "qualification output already exists: $work" >&2; exit 1; }
mkdir -p "$work"
export TMPDIR=/tmp
(cd "$package" && sha256sum -c payload.sha256) > "$work/package-checksums.log"
cp --sparse=always "$base" "$work/rootfs.ext4"
cp "$package/payload/vmlinux" "$work/vmlinux"
image=$work/rootfs.ext4
private=/home/dev/rem-native-userland/bootstrap/musl/native/libc.a
{
    echo "rm $private"
    echo "write $fold_libc $private"
    echo "sif $private mode 0100644"
    echo "sif $private uid 1000"
    echo "sif $private gid 100"
    echo "write $fold_libc /tmp/fold-libc-original.a"
} > "$work/baseline.debugfs"
debugfs -w -f "$work/baseline.debugfs" "$image" > "$work/baseline-write.log" 2>&1
debugfs -R "dump $private $work/baseline-libc.a" "$image" > "$work/baseline-read.log" 2>&1
cmp "$fold_libc" "$work/baseline-libc.a"
sha256sum "$fold_libc" "$work/baseline-libc.a" > "$work/baseline.sha256"
if [ -n "${PREVIOUS_PACKAGE:-}" ]; then
    before=$(sha256sum "$image" "$work/vmlinux")
    if sh "$PREVIOUS_PACKAGE/install.sh" install "$image" > "$work/previous-rejection.log" 2>&1; then
        echo "previous package unexpectedly accepted the actual Fold libc" >&2
        exit 1
    fi
    grep -Fq "unsupported or missing baseline for $private ($expected)" "$work/previous-rejection.log"
    [ "$before" = "$(sha256sum "$image" "$work/vmlinux")" ]
    [ ! -e "$work/update-20261008-v2-backup" ]
    echo "PASS previous release rejection reproduced with zero image/kernel mutation"
fi
sh "$package/install.sh" install "$image" > "$work/install.log" 2>&1
sh "$package/install.sh" verify "$image" > "$work/verify.log" 2>&1
sh "$package/install.sh" install "$image" > "$work/reinstall.log" 2>&1
cmp "$fold_libc" "$work/update-20261008-v2-backup/files/_home_dev_rem-native-userland_bootstrap_musl_native_libc.a"
debugfs -R "dump $private $work/updated-libc.a" "$image" > "$work/updated-read.log" 2>&1
cmp "$package/payload/libc.a" "$work/updated-libc.a"
cat > "$work/rc.local" <<'GUEST'
#!/bin/sh
exec > /dev/console 2>&1
PATH=/sbin:/bin:/usr/sbin:/usr/bin
export PATH
echo FOLD_REAL_LIBC_SYSV_BOOT
original=$(sha256sum /tmp/fold-libc-original.a | cut -d' ' -f1)
updated=$(sha256sum /home/dev/rem-native-userland/bootstrap/musl/native/libc.a | cut -d' ' -f1)
echo "FOLD_LIBC_INPUT_SHA256=$original"
echo "FOLD_LIBC_UPDATED_SHA256=$updated"
if [ "$original" != 0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186 ] ||
   [ "$updated" != 269b1d6b5a15a3e2b3c4f8fe48dc0f9f8b5922e2c90937586ec7167d22a1c7c2 ]; then
    echo 'REM_QA_DONE|1'
    exit 1
fi
sh /home/dev/rem-update-20261008/verify.sh full
rc=$?
sync
echo "REM_QA_DONE|$rc"
exit "$rc"
GUEST
{
    echo "rm /etc/rc.local"
    echo "write $work/rc.local /etc/rc.local"
    echo "sif /etc/rc.local mode 0100755"
} > "$work/qualification.debugfs"
debugfs -w -f "$work/qualification.debugfs" "$image" > "$work/qualification-write.log" 2>&1
e2fsck -fn "$image" > "$work/preboot-fsck.log" 2>&1
python3 - "$qemu" "$work" "${QUALIFICATION_TIMEOUT:-18000}" <<'HOST'
import datetime
import os
import pathlib
import re
import selectors
import subprocess
import sys
import time

qemu, directory, seconds = sys.argv[1:]
work = pathlib.Path(directory)
command = [
    qemu, "-M", "myemulator32", "-m", "128M", "-kernel", str(work / "vmlinux"),
    "-append", "console=ttyMY0,115200 earlycon=myemulator2,0xf0000000 root=/dev/myemu0 rw init=/sbin/init virtio_mmio.device=0x200@0xf0200000:5 ip=off",
    "-drive", f"file={work / 'rootfs.ext4'},format=raw,if=none,id=myemulator2-disk",
    "-nographic", "-monitor", "none", "-serial", "stdio", "-netdev", "user,id=net0",
    "-icount", "shift=0,sleep=off", "-no-reboot",
]
process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)
(work / "qemu.pid").write_text(str(process.pid) + "\n")
(work / "qemu-command.txt").write_text(" ".join(command) + "\n")
(work / "started.txt").write_text(datetime.datetime.now().astimezone().isoformat() + "\n")
print(f"QEMU_PID={process.pid} SERIAL={work / 'qualification.serial'}", flush=True)
deadline = time.monotonic() + int(seconds)
result = None
pending = b""
selector = selectors.DefaultSelector()
selector.register(process.stdout, selectors.EVENT_READ)
try:
    with (work / "qualification.serial").open("wb") as serial:
        while result is None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                print("FAIL qualification timed out", flush=True)
                result = 124
                break
            if not selector.select(min(remaining, 10)):
                if process.poll() is not None:
                    result = 1
                continue
            data = os.read(process.stdout.fileno(), 65536)
            if not data:
                print("FAIL QEMU exited without guest completion", flush=True)
                result = 1
                break
            serial.write(data)
            serial.flush()
            pending += data
            while b"\n" in pending:
                line, pending = pending.split(b"\n", 1)
                line = line.rstrip(b"\r")
                if re.search(rb"MYEMU_BAD_USER_FAULT|Kernel panic|Out of memory|Killed process|oom-kill|[Ss]egmentation fault|[Ii]llegal instruction|[Bb]us error|stack smashing detected|[Ff]loating point exception|[Aa]ssertion .* failed", line):
                    print("FAIL " + line.decode(errors="replace"), flush=True)
                    result = 1
                    break
                if line.startswith((b"COMPILER_REQUIRED_", b"SAMURAI_", b"REM_UPDATE_VERIFY_", b"REM_PROGRESS|", b"REM_LOG_TAIL|", b"PASS aggregate-small-return", b"FOLD_LIBC_")):
                    print(line.decode(errors="replace"), flush=True)
                match = re.fullmatch(rb"REM_QA_DONE\|([0-9]+)", line)
                if match:
                    result = int(match[1])
                    print(line.decode(), flush=True)
                    break
finally:
    selector.close()
    if process.poll() is None:
        process.terminate()
    try:
        qemu_result = process.wait(timeout=15)
    except subprocess.TimeoutExpired:
        process.kill()
        qemu_result = process.wait()
    (work / "qemu-exit.txt").write_text(str(qemu_result) + "\n")
if result is None:
    raise RuntimeError("qualification ended without a recorded result")
(work / "qualification-exit.txt").write_text(str(result) + "\n")
sys.exit(result)
HOST
tr -d '\r' < "$work/qualification.serial" > "$work/qualification.normalized"
for marker in COMPILER_REQUIRED_PORTING_TESTS_OK COMPILER_REQUIRED_SELFREBUILD_OK \
    SAMURAI_BUILD_OK SAMURAI_DRY_RUN_OK SAMURAI_NATIVE_BUILD_OK \
    SAMURAI_INCREMENTAL_REBUILD_OK SAMURAI_CLEAN_OK SAMURAI_NATIVE_EXECUTION_OK \
    REM_UPDATE_VERIFY_OK "REM_QA_DONE|0" "PASS aggregate-small-return (seed)" \
    "PASS aggregate-small-return (stage1)" "PASS aggregate-small-return (stage2)"; do
    grep -Fqx "$marker" "$work/qualification.normalized" ||
        { echo "missing actual native qualification marker: $marker" >&2; exit 1; }
done
echo "PASS exact Fold libc native qualification completed; logs: $work"
