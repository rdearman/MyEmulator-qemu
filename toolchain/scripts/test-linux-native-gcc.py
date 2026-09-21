#!/usr/bin/env python3
"""Compile and execute a C program with the staged MyEmulator2 Linux GCC."""

import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GCC = Path(os.environ.get("MYEMU_NATIVE_GCC", "/tmp/myemu-native/bin/myemulator2-linux-musl-gcc"))
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"
INITRAMFS = Path("/tmp/myemu-busybox-final-initramfs/initramfs.cpio")
GEN_INIT_CPIO = ROOT / ".linux-build/build/usr/gen_init_cpio"


def main():
    if not GCC.exists():
        raise SystemExit(f"missing native compiler: {GCC}")
    if not GEN_INIT_CPIO.exists():
        raise SystemExit(f"missing gen_init_cpio: {GEN_INIT_CPIO}")
    with tempfile.TemporaryDirectory(prefix="myemu-native-gcc-") as tmp:
        tmp = Path(tmp)
        source = tmp / "hello.c"
        binary = tmp / "hello"
        source.write_text('#include <stdio.h>\nint main(void) { puts("native-gcc-pass"); return 0; }\n')
        env = os.environ.copy()
        env["PATH"] = "/tmp/myemu-native/bin:/tmp/myemu-binutils-install/bin:" + env.get("PATH", "")
        subprocess.run([str(GCC), str(source), "-o", str(binary)], check=True, env=env)
        INITRAMFS.parent.mkdir(parents=True, exist_ok=True)
        listing = tmp / "init.list"
        listing.write_text(
            f"file /init {binary} 0755 0 0\n"
            "dir /dev 0755 0 0\n"
            "nod /dev/console 0600 0 0 c 5 1\n"
            "nod /dev/ttyMY0 0600 0 0 c 240 0\n")
        with INITRAMFS.open("wb") as out:
            subprocess.run([str(GEN_INIT_CPIO), str(listing)], check=True, stdout=out)
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")], check=True,
                       stdout=subprocess.DEVNULL)
        sock_path = tmp / "console.sock"
        proc = subprocess.Popen([
            str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel", str(KERNEL),
            "-nographic", "-monitor", "none", "-serial", "chardev:console",
            "-chardev", f"socket,id=console,path={sock_path},server=on,wait=on",
            "-icount", "shift=0,sleep=off"], stdout=subprocess.DEVNULL,
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
                        if sock is not None:
                            sock.close()
                        sock = None
                time.sleep(0.05)
            if sock is None:
                raise RuntimeError("QEMU console socket did not open")
            sock.settimeout(0.2)
            data = b""
            deadline = time.monotonic() + 30
            while b"native-gcc-pass" not in data and time.monotonic() < deadline:
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            if b"native-gcc-pass" not in data:
                raise RuntimeError("native GCC program did not execute")
        finally:
            if sock is not None:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    print("MyEmulator2 Linux native GCC: PASS")


if __name__ == "__main__":
    main()
