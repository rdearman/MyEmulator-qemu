#!/usr/bin/env python3
"""Verify the BusyBox shell over the real tty after an ext4 root boot."""

import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"
IMAGE = Path(os.environ.get("MYEMU_EXT4_IMAGE", "/tmp/myemu-ext4-busybox.img"))


def main():
    if not IMAGE.exists():
        raise SystemExit(f"missing disposable ext4 image: {IMAGE}")
    with tempfile.TemporaryDirectory(prefix="myemu-ext4-shell-") as tmp:
        sock_path = Path(tmp) / "console.sock"
        proc = subprocess.Popen([
            str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel", str(KERNEL),
            "-drive", f"file={IMAGE},format=raw,if=none,id=myemulator2-disk",
            "-nographic", "-monitor", "none", "-serial", "chardev:console",
            "-chardev", f"socket,id=console,path={sock_path},server=on,wait=on",
            "-icount", "shift=0,sleep=off"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        sock = None
        data = b""
        try:
            for _ in range(400):
                if sock_path.exists():
                    try:
                        sock = socket.socket(socket.AF_UNIX)
                        sock.connect(str(sock_path))
                        break
                    except OSError:
                        if sock:
                            sock.close()
                        sock = None
                time.sleep(.05)
            if sock is None:
                raise RuntimeError("console socket did not open")
            sock.settimeout(.2)
            deadline = time.monotonic() + 60
            while b"# " not in data and time.monotonic() < deadline:
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            if b"# " not in data:
                raise RuntimeError(f"ext4 BusyBox prompt not reached: {data[-2000:]!r}")
            commands = b"echo EXT4_READY\ncat /etc/hostname\n"
            commands += b"".join(f"echo EXT4_CMD{i:02d}\\n".encode() for i in range(1, 21))
            sock.sendall(commands)
            deadline = time.monotonic() + 60
            while (b"EXT4_READY" not in data or
                   any(f"EXT4_CMD{i:02d}".encode() not in data for i in range(1, 21))):
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"ext4 shell commands timed out: {data[-3000:]!r}")
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
        finally:
            if sock:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    print("MyEmulator2 ext4 BusyBox interactive shell: PASS")


if __name__ == "__main__":
    main()
