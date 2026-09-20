#!/usr/bin/env python3
"""Build and execute the repository's reusable static C runtime fixture."""
import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PREFIX = Path(os.environ.get("MYEMU_TOOLCHAIN_PREFIX", ROOT / ".toolchain-install"))
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"

def main():
    gcc = PREFIX / "bin/myemulator2-elf-gcc"
    as_ = PREFIX / "bin/myemulator2-elf-as"
    with tempfile.TemporaryDirectory(prefix="myemu-full-libc-") as tmp:
        out = Path(tmp)
        inc = ROOT / "toolchain/userspace/minilibc/include"
        cflags = ["-ffreestanding", "-fno-builtin", "-nostdinc", "-O2", "-I", str(inc)]
        sources = [("crt0-linux.S", "crt0.o"), ("syscall.S", "syscall.o"),
                   ("string.c", "string.o"), ("stdlib.c", "stdlib.o"),
                   ("posix.c", "posix.o"), ("stdio.c", "stdio.o")]
        for source, obj in sources:
            path = ROOT / "toolchain/userspace/minilibc" / source
            if source.endswith(".S"):
                subprocess.run([str(as_), "-o", str(out / obj), str(path)], check=True)
            else:
                subprocess.run([str(gcc), *cflags, "-c", str(path), "-o", str(out / obj)], check=True)
        test_obj = out / "test.o"
        subprocess.run([str(gcc), *cflags, "-c", str(ROOT / "toolchain/examples/linux-libc-regression.c"), "-o", str(test_obj)], check=True)
        subprocess.run([str(gcc), "-nostdlib", "-Ttext=0x00500000", "-o", str(out / "init.elf"),
                        *[str(out / obj) for _, obj in sources], str(test_obj), "-lgcc"], check=True)
        list_path = Path("/tmp/myemu-initramfs/list")
        list_path.parent.mkdir(parents=True, exist_ok=True)
        list_path.write_text(f"file /init {out / 'init.elf'} 0755 0 0\n"
                             f"file /test.txt {out / 'test.txt'} 0644 0 0\n"
                             "dir /dev 0755 0 0\n"
                             "nod /dev/console 0600 0 0 c 5 1\n"
                             "nod /dev/ttyMY0 0600 0 0 c 240 0\n")
        (out / "test.txt").write_text("hello from initramfs\n")
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")], check=True, stdout=subprocess.DEVNULL)
        sock_path = out / "console.sock"
        proc = subprocess.Popen([str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel", str(KERNEL),
                                 "-nographic", "-monitor", "none", "-serial", "chardev:console",
                                 "-chardev", f"socket,id=console,path={sock_path},server=on,wait=on",
                                 "-icount", "shift=0,sleep=off"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        sock = None
        try:
            for _ in range(300):
                if sock_path.exists():
                    try:
                        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                        sock.connect(str(sock_path)); break
                    except OSError:
                        if sock: sock.close()
                        sock = None
                time.sleep(.05)
            if not sock: raise RuntimeError("console socket did not open")
            sock.settimeout(.2); data = b""; deadline = time.monotonic() + 300
            while b"LIBC REGRESSION PASS" not in data and time.monotonic() < deadline:
                try: data += sock.recv(8192)
                except socket.timeout: pass
            if b"LIBC REGRESSION PASS" not in data:
                raise RuntimeError("full minilibc regression did not pass: " + data[-500:].decode(errors="replace"))
        finally:
            if sock: sock.close()
            proc.terminate()
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()
    print("MyEmulator2 reusable static C library: PASS")

if __name__ == "__main__": main()
