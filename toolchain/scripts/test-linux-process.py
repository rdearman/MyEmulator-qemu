#!/usr/bin/env python3
"""Run the MyEmulator2 clone/exit/wait4 regression under QEMU."""

import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PREFIX = Path(os.environ.get(
    "MYEMU_TOOLCHAIN_PREFIX", ROOT / ".toolchain-install"))
QEMU = Path(os.environ.get(
    "QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"
INIT_LIST = Path("/tmp/myemu-initramfs/list")


def recv_until(sock, data, needle, deadline):
    while needle not in data and time.monotonic() < deadline:
        try:
            data += sock.recv(8192)
        except socket.timeout:
            pass
    return data


def main():
    with tempfile.TemporaryDirectory(prefix="myemu-process-") as tmp:
        out = Path(tmp)
        obj = out / "linux-clone-wait.o"
        elf = out / "linux-clone-wait.elf"
        subprocess.run([
            str(PREFIX / "bin/myemulator2-elf-as"), "-o", str(obj),
            str(ROOT / "toolchain/examples/linux-clone-wait.S")], check=True)
        subprocess.run([
            str(PREFIX / "bin/myemulator2-elf-ld"), "-Ttext=0x00500000",
            "-o", str(elf), str(obj)], check=True)

        INIT_LIST.parent.mkdir(parents=True, exist_ok=True)
        INIT_LIST.write_text(
            f"file /init {elf} 0755 0 0\n"
            "dir /dev 0755 0 0\n"
            "nod /dev/console 0600 0 0 c 5 1\n"
            "nod /dev/ttyMY0 0600 0 0 c 240 0\n")
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")],
                       check=True, stdout=subprocess.DEVNULL)

        sock_path = out / "console.sock"
        proc = subprocess.Popen([
            str(QEMU), "-M", "myemulator32", "-m", "16M",
            "-kernel", str(KERNEL), "-nographic", "-monitor", "none",
            "-serial", "chardev:console", "-chardev",
            f"socket,id=console,path={sock_path},server=on,wait=on",
            "-icount", "shift=0,sleep=off"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        sock = None
        try:
            for _ in range(300):
                if sock_path.exists():
                    try:
                        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                        sock.connect(str(sock_path))
                        break
                    except OSError:
                        if sock is not None:
                            sock.close()
                        sock = None
                time.sleep(0.05)
            if sock is None:
                raise RuntimeError("QEMU console socket did not open")
            sock.settimeout(0.2)
            data = recv_until(sock, b"", b"process parent waited", 
                              time.monotonic() + 30)
            for text in (b"process test start", b"process child",
                         b"process parent waited"):
                if text not in data:
                    raise RuntimeError(f"missing process output: {text!r}")
            if b"unhandled MyEmulator2 exception" in data:
                raise RuntimeError("unhandled MyEmulator2 exception")
        finally:
            if sock is not None:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    print("MyEmulator2 Linux process clone/exit/wait4: PASS")


if __name__ == "__main__":
    main()
