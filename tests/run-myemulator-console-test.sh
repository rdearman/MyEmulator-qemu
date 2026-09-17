#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
qemu=${QEMU_MYEMULATOR:-$root/.qemu-build/qemu-system-myemulator}
test -x "$qemu"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-console.XXXXXX")
pid=
cleanup() { test -z "$pid" || kill "$pid" 2>/dev/null || true; test -z "$pid" || wait "$pid" 2>/dev/null || true; rm -rf "$tmp"; }
trap cleanup EXIT HUP INT TERM

"$root/tools/myasm" "$root/examples/asm/console-output.asm" -o "$tmp/output.bin"
"$root/tools/myasm" "$root/examples/asm/console-echo.asm" -o "$tmp/echo.bin"
"$root/tools/myasm" "$root/examples/asm/console-interrupt.asm" -o "$tmp/interrupt.bin" --flat-64k

"$qemu" -M myemulator -kernel "$tmp/output.bin" -nographic -monitor none \
  -serial none -chardev "socket,id=console,path=$tmp/output.sock,server=on,wait=on" \
  >"$tmp/qemu.out" 2>"$tmp/qemu.err" &
pid=$!
python3 - "$tmp/output.sock" <<'PY'
import socket, sys, time
path = sys.argv[1]
for _ in range(100):
    try:
        s = socket.socket(socket.AF_UNIX)
        s.connect(path)
        break
    except OSError:
        time.sleep(0.02)
else:
    raise SystemExit("console socket did not open")
s.settimeout(2)
data = b""
while len(data) < 3:
    data += s.recv(3 - len(data))
assert data == b"RIK", data
s.close()
PY
kill "$pid"
wait "$pid" 2>/dev/null || true
pid=

"$qemu" -M myemulator -kernel "$tmp/echo.bin" -nographic -monitor none \
  -serial none -chardev "socket,id=console,path=$tmp/echo.sock,server=on,wait=on" \
  >"$tmp/qemu.out" 2>"$tmp/qemu.err" &
pid=$!
python3 - "$tmp/echo.sock" <<'PY'
import socket, sys, time
path = sys.argv[1]
for _ in range(100):
    try:
        s = socket.socket(socket.AF_UNIX)
        s.connect(path)
        break
    except OSError:
        time.sleep(0.02)
else:
    raise SystemExit("console socket did not open")
s.settimeout(2)
s.sendall(b"A")
assert s.recv(1) == b"A"
s.close()
PY
kill "$pid"
wait "$pid" 2>/dev/null || true
pid=
echo 'MyEmulator console output and polling echo tests passed'

"$qemu" -M myemulator -S -kernel "$tmp/interrupt.bin" -nographic \
  -monitor none -serial none \
  -chardev "socket,id=console,path=$tmp/interrupt.sock,server=on,wait=on" \
  -qmp "unix:$tmp/interrupt.qmp,server=on,wait=off" \
  >"$tmp/qemu.out" 2>"$tmp/qemu.err" &
pid=$!
python3 - "$tmp/interrupt.sock" "$tmp/interrupt.qmp" <<'PY'
import json, socket, sys, time

def connect(path):
    for _ in range(100):
        try:
            s = socket.socket(socket.AF_UNIX)
            s.connect(path)
            return s
        except OSError:
            time.sleep(0.02)
    raise SystemExit("socket did not open: " + path)

console = connect(sys.argv[1])
qmp = connect(sys.argv[2])
qmp.settimeout(2)
qmp_buffer = b""

def response():
    global qmp_buffer
    while True:
        while b"\r\n" not in qmp_buffer:
            qmp_buffer += qmp.recv(4096)
        line, qmp_buffer = qmp_buffer.split(b"\r\n", 1)
        item = json.loads(line)
        if "QMP" in item or "return" in item or "error" in item:
            return item

response()  # greeting
qmp.sendall(b'{"execute":"qmp_capabilities"}\r\n')
response()

def debug(op, **args):
    request = {"execute": "myemulator-debug",
               "arguments": {"op": op, **args}}
    qmp.sendall((json.dumps(request) + "\r\n").encode())
    item = response()
    if "error" in item:
        raise SystemExit(item)
    return item["return"]

debug("continue")
console.sendall(b"Z")
time.sleep(0.1)
debug("stop")
memory = debug("read-memory", address=0x0200, length=1)
assert memory["data"] == "5a", memory
console.close()
qmp.close()
PY
kill "$pid"
wait "$pid" 2>/dev/null || true
pid=
echo 'MyEmulator console interrupt test passed'
