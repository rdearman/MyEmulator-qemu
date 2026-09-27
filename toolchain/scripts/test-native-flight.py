#!/usr/bin/env python3
import hashlib
import pexpect
import shutil
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
QEMU = ROOT / ".qemu-build/qemu-system-myemulator32"
KERNEL = ROOT / ".linux-build/build/vmlinux"
IMAGE = Path("/tmp/rem-flight-native-tools.ext4")
LOG = Path("/tmp/rem-flight-native-tools.log")

def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

args = [str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel", str(KERNEL),
        "-drive", f"file={IMAGE},format=raw,if=none,id=myemulator2-disk",
        "-nographic", "-monitor", "none", "-serial", "stdio",
        "-icount", "shift=0,sleep=off"]
child = pexpect.spawn(args[0], args[1:], timeout=10, encoding="utf-8",
                      codec_errors="replace")
with LOG.open("w") as log:
    child.logfile = log
    try:
        child.expect("# ", timeout=60)
        commands = [
            "echo GUEST_READY",
            "/root/native/bin/as -o /root/hello.o /root/hello.S; echo AS_RC:$?",
            "cp /root/hello.o /root/hello.copy; sync; echo COPY_RC:$?",
            "/root/native/bin/ld -T /root/native/lib/ldscripts/elf32myemulator2.x -o /root/hello /root/hello.o; echo LD_RC:$?",
            "/root/hello; echo RUN_RC:$?",
        ]
        for command in commands:
            child.sendline(command)
            child.expect("# ", timeout=120)
        child.sendline("exit")
        child.expect(pexpect.EOF, timeout=10)
    finally:
        child.close(force=True)

print(f"qemu_sha256={sha(QEMU)}")
print(f"kernel_sha256={sha(KERNEL)}")
print(f"rootfs_sha256={sha(IMAGE)}")
print(f"log={LOG}")
