#!/usr/bin/env python3
"""Exercise the bootstrap shell with serialized commands over QEMU UART."""

import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
QEMU = Path(os.environ.get(
    "QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = Path(os.environ.get("MYEMU_LINUX_KERNEL",
                             ROOT / ".linux-build/build/vmlinux"))


def read_until(sock, buffer, marker, timeout):
    end = time.monotonic() + timeout
    while marker not in buffer and time.monotonic() < end:
        sock.settimeout(min(2.0, max(0.1, end - time.monotonic())))
        try:
            data = sock.recv(4096)
        except socket.timeout:
            continue
        if not data:
            break
        buffer += data
    if marker not in buffer:
        return buffer, None
    end_marker = buffer.index(marker) + len(marker)
    return buffer[end_marker:], buffer[:end_marker]


def main():
    with tempfile.TemporaryDirectory(prefix="myemu-shell-test-") as tmp:
        socket_path = Path(tmp) / "console.sock"
        command = [
            str(QEMU), "-M", "myemulator32", "-m", "16M",
            "-kernel", str(KERNEL), "-nographic", "-monitor", "none",
            "-serial", "chardev:console", "-chardev",
            f"socket,id=console,path={socket_path},server=on,wait=on",
        ]
        proc = subprocess.Popen(command, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        sock = None
        try:
            deadline = time.monotonic() + 300
            while time.monotonic() < deadline:
                if socket_path.exists():
                    try:
                        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                        sock.connect(str(socket_path))
                        break
                    except OSError:
                        sock.close()
                        sock = None
                time.sleep(0.2)
            if sock is None:
                raise RuntimeError("QEMU console socket did not open")

            buffer, output = read_until(sock, b"", b"myemu> ", 300)
            if output is None:
                raise RuntimeError("bootstrap shell prompt did not appear")

            expected = b"help echo uname clear exit"
            for index in range(20):
                sock.sendall(b"help\n")
                buffer, output = read_until(sock, buffer, b"myemu> ", 30)
                if output is None or expected not in output:
                    raise RuntimeError(
                        f"command {index + 1} did not return expected output: "
                        f"{output!r}")
        finally:
            if sock is not None:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

    print("MyEmulator2 bootstrap shell: 20 consecutive commands PASS")


if __name__ == "__main__":
    main()
