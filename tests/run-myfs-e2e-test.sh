#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
qemu=${QEMU_MYEMULATOR:-$root/.qemu-build/qemu-system-myemulator}
test -x "$qemu"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-myfs.XXXXXX")
pid=
cleanup() {
    test -z "$pid" || kill "$pid" 2>/dev/null || true
    test -z "$pid" || wait "$pid" 2>/dev/null || true
    rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM

"$qemu" -M myemulator -S -kernel "$root/build/myfs/kernel.bin" \
    -nographic -monitor none -serial none \
    -chardev "socket,id=console,path=$tmp/console.sock,server=on,wait=off" \
    -drive "file=$root/build/myfs/myemulator.img,format=raw,if=none,id=myemulator-floppy" \
    -qmp "unix:$tmp/qmp.sock,server=on,wait=off" \
    >"$tmp/qemu.out" 2>"$tmp/qemu.err" &
pid=$!

python3 - "$tmp/console.sock" "$tmp/qmp.sock" <<'PY'
import json
import socket
import sys
import time


def connect(path):
    for _ in range(200):
        try:
            sock = socket.socket(socket.AF_UNIX)
            sock.connect(path)
            sock.settimeout(3)
            return sock
        except OSError:
            time.sleep(0.02)
    raise SystemExit(f"socket did not open: {path}")


console = connect(sys.argv[1])
qmp = connect(sys.argv[2])
qmp_buffer = b""


def qmp_read():
    global qmp_buffer
    while b"\r\n" not in qmp_buffer:
        qmp_buffer += qmp.recv(4096)
    line, qmp_buffer = qmp_buffer.split(b"\r\n", 1)
    return json.loads(line)


qmp_read()  # greeting
qmp.sendall(b'{"execute":"qmp_capabilities"}\r\n')
while "return" not in qmp_read():
    pass


def debug(op, **arguments):
    qmp.sendall((json.dumps({
        "execute": "myemulator-debug",
        "arguments": {"op": op, **arguments},
    }) + "\r\n").encode())
    while True:
        response = qmp_read()
        if response.get("event"):
            continue
        if "error" in response:
            raise SystemExit(response)
        return response["return"]


def until_prompt():
    data = b""
    while b"A:\\> " not in data:
        data += console.recv(4096)
    return data


debug("continue")
assert b"A:\\> " in until_prompt()
for command, expected in (
    (b"DIR\n", (b"COMMAND COM 06A5", b"HELLO   COM 0031", b"README  TXT 0053")),
    (b"TYPE README.TXT\n", (b"This file was read from a contiguous read-only filesystem.",)),
    (b"RUN HELLO.COM\n", (b"Hello from MyEmulator!",)),
    (b"VER\n", (b"MyEmulator 1.0", b"MyFS 1.0")),
    (b"HELP\n", (b"DIR TYPE RUN CLS VER HELP",)),
):
    console.sendall(command)
    output = until_prompt()
    for text in expected:
        assert text in output, (command, output, text)

console.close()
qmp.close()
PY

echo 'MyFS end-to-end boot, DIR, TYPE, RUN, VER, and HELP test passed'
