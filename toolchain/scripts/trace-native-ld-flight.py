#!/usr/bin/env python3
import pexpect
from pathlib import Path

QEMU = Path("/tmp/rem-qemu-trace-build/qemu-system-myemulator32")
KERNEL = Path("/tmp/rem-linux-align-build/vmlinux")
IMAGE = Path("/tmp/rem-flight-native-tools.ext4")
LOG = Path("/tmp/rem-flight-native-ld-trace.log")

args = [str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel", str(KERNEL),
        "-drive", f"file={IMAGE},format=raw,if=none,id=myemulator2-disk",
        "-nographic", "-monitor", "none", "-serial", "stdio",
        "-icount", "shift=0,sleep=off"]
child = pexpect.spawn(args[0], args[1:], timeout=5, encoding="utf-8",
                      codec_errors="replace", env={"MYEMU_TRACE_R5": "1"})
with LOG.open("w") as log:
    child.logfile = log
    try:
        child.expect("# ", timeout=60)
        child.sendline("/root/native/bin/ld -T /root/native/lib/ldscripts/elf32myemulator2.x -o /root/hello /root/hello.o; echo LD_RC:$?")
        try:
            child.expect("LD_RC:", timeout=120)
            child.expect("# ", timeout=10)
        except (pexpect.TIMEOUT, pexpect.EOF):
            pass
    finally:
        child.close(force=True)
print(LOG)
