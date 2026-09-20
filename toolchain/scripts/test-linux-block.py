#!/usr/bin/env python3
"""Exercise MyEmulator2 block I/O through the Linux block layer."""

import os
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
BUILD = ROOT / ".linux-build/build"
LINUX = ROOT / ".linux-build/linux-6.12.1"


def main():
    with tempfile.TemporaryDirectory(prefix="myemu-block-test-") as tmp:
        out = Path(tmp)
        gcc = PREFIX / "bin/myemulator2-elf-gcc"
        as_ = PREFIX / "bin/myemulator2-elf-as"
        ld = PREFIX / "bin/myemulator2-elf-ld"
        cflags = ["-ffreestanding", "-fno-builtin", "-nostdinc", "-O2",
                  "-I", str(ROOT / "toolchain/userspace/minilibc/include")]
        for source, name in (("string.c", "string.o"),
                             ("linux-block.c", "main.o")):
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
        subprocess.run([str(ld), "-Ttext=0x00500000", "-o",
                        str(out / "init.elf"), str(out / "crt0.o"),
                        str(out / "syscall.o"), str(out / "string.o"),
                        str(out / "main.o")], check=True)

        init_list = out / "initramfs.list"
        init_list.write_text(
            f"file /init {out / 'init.elf'} 0755 0 0\n"
            "dir /dev 0755 0 0\n"
            "nod /dev/ttyMY0 0600 0 0 c 240 0\n"
            "nod /dev/myemu0 0600 0 0 b 259 0\n")
        config = LINUX / "scripts/config"
        subprocess.run([str(config), "--file", str(BUILD / ".config"),
                        "--set-str", "INITRAMFS_SOURCE", str(init_list)],
                       check=True)
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")],
                       check=True, stdout=subprocess.DEVNULL)

        image = out / "disk.img"
        with image.open("wb") as disk:
            disk.truncate(1024 * 1024)
        serial = out / "serial.log"
        proc = subprocess.Popen([
            str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel",
            str(KERNEL), "-drive",
            f"file={image},format=raw,if=none,id=myemulator2-disk",
            "-nographic", "-monitor", "none", "-serial", f"file:{serial}",
            "-icount", "shift=0,sleep=off"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                data = serial.read_bytes() if serial.exists() else b""
                if b"BLOCK RW PASS" in data:
                    break
                if b"BLOCK OPEN FAIL" in data or b"BLOCK WRITE FAIL" in data \
                        or b"BLOCK READ FAIL" in data or b"BLOCK DATA FAIL" in data:
                    raise RuntimeError(data[-2000:].decode("latin1", "replace"))
                time.sleep(0.2)
            else:
                raise TimeoutError((serial.read_bytes() if serial.exists() else b"")[-2000:])
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

        payload = bytes((i ^ 0xA5) & 0xFF for i in range(512))
        with image.open("rb") as disk:
            disk.seek(512)
            if disk.read(512) != payload:
                raise RuntimeError("disk image did not retain sector data")
    print("MyEmulator2 Linux block read/write test: PASS")


if __name__ == "__main__":
    main()
