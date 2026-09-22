#!/usr/bin/env python3
"""Compile and execute a C program with the REM Linux minilibc."""

import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PREFIX = Path(os.environ.get("MYEMU_TOOLCHAIN_PREFIX",
                           ROOT / ".toolchain-install"))
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32",
                          ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"
LINUX = ROOT / ".linux-build/linux-6.12.1"
BUILD = ROOT / ".linux-build/build"
INIT_LIST = Path("/tmp/myemu-initramfs/list")


def main():
    gcc = PREFIX / "bin/myemulator2-elf-gcc"
    as_ = PREFIX / "bin/myemulator2-elf-as"
    ld = PREFIX / "bin/myemulator2-elf-ld"
    with tempfile.TemporaryDirectory(prefix="myemu-minilibc-") as tmp:
        out = Path(tmp)
        cflags = ["-ffreestanding", "-fno-builtin", "-nostdinc", "-O2",
                  "-I", str(ROOT / "toolchain/userspace/minilibc/include")]
        subprocess.run([str(gcc), *cflags, "-c",
                        str(ROOT / "toolchain/userspace/minilibc/string.c"),
                        "-o", str(out / "string.o")], check=True)
        subprocess.run([str(gcc), *cflags, "-c",
                        str(ROOT / "toolchain/examples/linux-minilibc.c"),
                        "-o", str(out / "main.o")], check=True)
        for source, name in (("crt0-linux.S", "crt0.o"),
                             ("syscall.S", "syscall.o")):
            subprocess.run([str(as_), "-o", str(out / name),
                            str(ROOT / "toolchain/userspace/minilibc" / source)],
                           check=True)
        subprocess.run([str(ld), "-Ttext=0x02000000", "-o",
                        str(out / "init.elf"), str(out / "crt0.o"),
                        str(out / "syscall.o"), str(out / "string.o"),
                        str(out / "main.o")], check=True)
        INIT_LIST.parent.mkdir(parents=True, exist_ok=True)
        INIT_LIST.write_text(
            f"file /init {out / 'init.elf'} 0755 0 0\n"
            "dir /dev 0755 0 0\n"
            "nod /dev/console 0600 0 0 c 5 1\n"
            "nod /dev/ttyMY0 0600 0 0 c 240 0\n")
        subprocess.run([str(LINUX / "scripts/config"), "--file",
                        str(BUILD / ".config"), "--set-str",
                        "INITRAMFS_SOURCE", str(INIT_LIST)], check=True)
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")],
                       check=True, stdout=subprocess.DEVNULL)

        sock_path = out / "console.sock"
        proc = subprocess.Popen([
            str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel",
            str(KERNEL), "-nographic", "-monitor", "none", "-serial",
            "chardev:console", "-chardev",
            f"socket,id=console,path={sock_path},server=on,wait=on",
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
            deadline = time.monotonic() + 120
            while b"C syscall wrappers reached" not in data and time.monotonic() < deadline:
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            for text in (b"C minilibc reached", b"C syscall wrappers reached"):
                if text not in data:
                    raise RuntimeError(f"missing minilibc output: {text!r}; output tail={data[-2000:]!r}")
            if b"page fault" in data or b"unhandled REM exception" in data:
                raise RuntimeError("kernel exception during minilibc test")
        finally:
            if sock is not None:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    print("REM Linux C minilibc syscall test: PASS")


if __name__ == "__main__":
    main()
