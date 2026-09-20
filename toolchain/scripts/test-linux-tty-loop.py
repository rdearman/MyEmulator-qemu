#!/usr/bin/env python3
"""Build and exercise the real Linux serial-core TTY with 20 input lines."""

import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32",
                           ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"


def main():
    with tempfile.TemporaryDirectory(prefix="myemu-tty-loop-") as tmp:
        out = Path(tmp)
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux-tty-loop-initramfs.sh"),
                        str(out)], check=True)
        initlist = Path("/tmp/myemu-initramfs/list")
        initlist.parent.mkdir(parents=True, exist_ok=True)
        initlist.write_text(
            f"file /init {out / 'linux-tty-loop.elf'} 0755 0 0\n"
            "dir /dev 0755 0 0\n"
            "nod /dev/console 0600 0 0 c 5 1\n"
            "nod /dev/ttyMY0 0600 0 0 c 240 0\n")
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")], check=True,
                       stdout=subprocess.DEVNULL)

        sock_path = out / "console.sock"
        command = [str(QEMU), "-M", "myemulator32", "-m", "16M",
                   "-kernel", str(KERNEL), "-nographic", "-monitor", "none",
                   "-serial", "chardev:console", "-chardev",
                   f"socket,id=console,path={sock_path},server=on,wait=on",
                   "-icount", "shift=0,sleep=off"]
        proc = subprocess.Popen(command, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        sock = None
        try:
            for _ in range(300):
                if sock_path.exists():
                    try:
                        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                        sock.connect(str(sock_path))
                        break
                    except OSError:
                        sock.close()
                        sock = None
                time.sleep(0.05)
            if sock is None:
                raise RuntimeError("QEMU console socket did not open")

            sock.settimeout(0.2)
            data = b""
            deadline = time.monotonic() + 20
            while b"TTY loop ready" not in data and time.monotonic() < deadline:
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            if b"TTY loop ready" not in data:
                raise RuntimeError("TTY fixture did not start")
            sock.sendall(b"".join(f"cmd{i:02d}\n".encode() for i in range(20)))
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                if all(f"cmd{i:02d}".encode() in data for i in range(20)):
                    break
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            missing = [f"cmd{i:02d}" for i in range(20)
                       if f"cmd{i:02d}".encode() not in data]
            if missing:
                raise RuntimeError(f"missing TTY echoes: {missing}")
            if b"TTY loop FAILED" in data:
                raise RuntimeError("guest TTY fixture reported failure")
        finally:
            if sock is not None:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    print("MyEmulator2 Linux TTY: 20 consecutive input lines PASS")


if __name__ == "__main__":
    main()
