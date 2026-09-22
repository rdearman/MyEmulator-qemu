#!/usr/bin/env python3
"""Verify guest-created ext4 data across two QEMU boots."""

import os
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PREFIX = Path(os.environ.get("MYEMU_TOOLCHAIN_PREFIX", ROOT / ".toolchain-install"))
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"
BUILD = ROOT / ".linux-build/build"
LINUX = ROOT / ".linux-build/linux-6.12.1"


def build_init(out):
    gcc = PREFIX / "bin/myemulator2-elf-gcc"
    as_ = PREFIX / "bin/myemulator2-elf-as"
    ld = PREFIX / "bin/myemulator2-elf-ld"
    flags = ["-ffreestanding", "-fno-builtin", "-nostdinc", "-O2", "-I",
             str(ROOT / "toolchain/userspace/minilibc/include")]
    subprocess.run([str(gcc), *flags, "-c", str(ROOT / "toolchain/userspace/minilibc/string.c"),
                    "-o", str(out / "string.o")], check=True)
    subprocess.run([str(gcc), *flags, "-c", str(ROOT / "toolchain/examples/linux-ext4-persist.c"),
                    "-o", str(out / "main.o")], check=True)
    for source, name in (("crt0-linux.S", "crt0.o"), ("syscall.S", "syscall.o")):
        subprocess.run([str(as_), "-o", str(out / name),
                        str(ROOT / "toolchain/userspace/minilibc" / source)], check=True)
    subprocess.run([str(ld), "-Ttext=0x02000000", "-o", str(out / "init.elf"),
                    str(out / "crt0.o"), str(out / "syscall.o"),
                    str(out / "string.o"), str(out / "main.o")], check=True)


def boot(image, serial):
    proc = subprocess.Popen([str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel",
                             str(KERNEL), "-drive",
                             f"file={image},format=raw,if=none,id=myemulator2-disk",
                             "-nographic", "-monitor", "none", "-serial", f"file:{serial}",
                             "-icount", "shift=0,sleep=off"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            data = serial.read_bytes() if serial.exists() else b""
            if b"PERSIST CREATED" in data or b"PERSIST PASS" in data:
                return data
            if any(marker in data for marker in (b"PERSIST OPEN FAIL", b"PERSIST WRITE FAIL",
                                                  b"PERSIST FSYNC FAIL", b"PERSIST READ FAIL",
                                                  b"PERSIST DATA FAIL")):
                raise RuntimeError(data[-2000:].decode("latin1", "replace"))
            time.sleep(0.2)
        raise TimeoutError((serial.read_bytes() if serial.exists() else b"")[-2000:])
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def main():
    with tempfile.TemporaryDirectory(prefix="myemu-ext4-persist-") as tmp:
        out = Path(tmp)
        build_init(out)
        rootfs = out / "rootfs"
        (rootfs / "sbin").mkdir(parents=True)
        (rootfs / "dev").mkdir()
        (rootfs / "root").mkdir()
        os.chmod(out / "init.elf", 0o755)
        os.link(out / "init.elf", rootfs / "sbin/init")
        image = out / "disk.img"
        subprocess.run(["truncate", "-s", "16M", str(image)], check=True)
        subprocess.run(["mkfs.ext4", "-q", "-F", "-d", str(rootfs), str(image)], check=True)
        config = LINUX / "scripts/config"
        subprocess.run([str(config), "--file", str(BUILD / ".config"),
                        "--set-str", "INITRAMFS_SOURCE", ""], check=True)
        subprocess.run([str(config), "--file", str(BUILD / ".config"),
                        "--enable", "EXT4_FS"], check=True)
        subprocess.run([str(config), "--file", str(BUILD / ".config"),
                        "--set-str", "CMDLINE",
                        "console=myemu0,115200 earlycon=myemu32,0xf0000000 root=/dev/myemu0 rw init=/sbin/init"], check=True)
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")], check=True,
                       stdout=subprocess.DEVNULL)
        first = out / "first.log"
        second = out / "second.log"
        first_data = boot(image, first)
        if b"PERSIST CREATED" not in first_data:
            raise RuntimeError("first boot did not create the file")
        second_data = boot(image, second)
        if b"PERSIST PASS" not in second_data:
            raise RuntimeError("second boot did not read the guest-created file")
    print("REM ext4 guest persistence: PASS")


if __name__ == "__main__":
    main()
