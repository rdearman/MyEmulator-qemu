#!/usr/bin/env python3
"""Run a bounded BusyBox shell regression over the MyEmulator2 TTY."""

import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"


def main():
    with tempfile.TemporaryDirectory(prefix="myemu-busybox-shell-") as tmp:
        path = Path(tmp) / "console.sock"
        proc = subprocess.Popen([
            str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel", str(KERNEL),
            "-nographic", "-monitor", "none", "-serial", "chardev:console",
            "-chardev", f"socket,id=console,path={path},server=on,wait=on",
            "-icount", "shift=0,sleep=off"], stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL)
        sock = None
        data = b""
        try:
            for _ in range(400):
                if path.exists():
                    try:
                        sock = socket.socket(socket.AF_UNIX)
                        sock.connect(str(path))
                        break
                    except OSError:
                        if sock:
                            sock.close()
                        sock = None
                time.sleep(.05)
            if sock is None:
                raise RuntimeError("console socket did not open")
            sock.settimeout(.2)
            deadline = time.monotonic() + float(os.environ.get("MYEMU_SHELL_PROMPT_TIMEOUT", "180"))
            while b"# " not in data and time.monotonic() < deadline:
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            if b"# " not in data:
                raise RuntimeError(f"BusyBox prompt not reached: {data[-2000:]!r}")
            commands = (
                b"echo READY\n"
                b"uname -a\n"
                b"mkdir /tmp/test\n"
                b"echo data > /tmp/test/file\n"
                b"cat /tmp/test/file\n"
                b"cp /tmp/test/file /tmp/test/copy\n"
                b"mv /tmp/test/copy /tmp/test/renamed\n"
                b"rm /tmp/test/renamed\n"
                b"rmdir /tmp/test\n"
                b"echo DONE\n"
            )
            commands += b"".join(f"echo CMD{i:02d}\\n".encode() for i in range(1, 21))
            sock.sendall(commands)
            deadline = time.monotonic() + 180
            while (b"READY" not in data or b"DONE" not in data or
                   any(f"CMD{i:02d}".encode() not in data for i in range(1, 21))):
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"shell commands timed out: {data[-3000:]!r}")
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            if b"data" not in data:
                raise RuntimeError(f"file content missing: {data[-3000:]!r}")
        finally:
            if sock:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    print("MyEmulator2 BusyBox interactive shell: PASS")


if __name__ == "__main__":
    main()
