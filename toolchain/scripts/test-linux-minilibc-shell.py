#!/usr/bin/env python3
"""Execute a small hosted C shell through Linux's MyEmulator2 console ABI."""

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
INIT_LIST = Path("/tmp/myemu-initramfs/list")


def main():
    gcc = PREFIX / "bin/myemulator2-elf-gcc"
    as_ = PREFIX / "bin/myemulator2-elf-as"
    ld = PREFIX / "bin/myemulator2-elf-ld"
    with tempfile.TemporaryDirectory(prefix="myemu-minilibc-shell-") as tmp:
        out = Path(tmp)
        cflags = ["-ffreestanding", "-fno-builtin", "-nostdinc", "-O2",
                  "-I", str(ROOT / "toolchain/userspace/minilibc/include")]
        for source, name in (("string.c", "string.o"),
                             ("linux-minilibc-shell.c", "shell.o")):
            source_path = (ROOT / "toolchain/userspace/minilibc" / source
                           if source == "string.c" else
                           ROOT / "toolchain/examples" / source)
            subprocess.run([str(gcc), *cflags, "-c", str(source_path),
                            "-o", str(out / name)], check=True)
        for source, name in (("crt0-linux.S", "crt0.o"),
                             ("syscall.S", "syscall.o")):
            subprocess.run([str(as_), "-o", str(out / name),
                            str(ROOT / "toolchain/userspace/minilibc" / source)],
                           check=True)
        subprocess.run([str(ld), "-Ttext=0x00500000", "-o", str(out / "init.elf"),
                        str(out / "crt0.o"), str(out / "syscall.o"),
                        str(out / "string.o"), str(out / "shell.o")], check=True)
        INIT_LIST.parent.mkdir(parents=True, exist_ok=True)
        INIT_LIST.write_text(
            f"file /init {out / 'init.elf'} 0755 0 0\n"
            "dir /dev 0755 0 0\n"
            "nod /dev/console 0600 0 0 c 5 1\n"
            "nod /dev/ttyMY0 0600 0 0 c 240 0\n")
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
            deadline = time.monotonic() + 30
            while b"myemu-c> " not in data and time.monotonic() < deadline:
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            if b"myemu-c> " not in data:
                raise RuntimeError("C shell prompt did not appear")
            for command, response in ((b"help\n", b"help echo uname mem exit"),
                                       (b"echo hello\n", b"hello"),
                                       (b"uname\n", b"MyEmulator2 Linux 6.12.1"),
                                       (b"mem\n", b"userspace memory OK")):
                sock.sendall(command)
                end = time.monotonic() + 10
                while response not in data and time.monotonic() < end:
                    try:
                        data += sock.recv(8192)
                    except socket.timeout:
                        pass
                if response not in data:
                    raise RuntimeError(f"shell response missing: {response!r}")
        finally:
            if sock is not None:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    print("MyEmulator2 Linux hosted C shell: PASS")


if __name__ == "__main__":
    main()
