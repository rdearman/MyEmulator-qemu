#!/usr/bin/env python3
"""Boot and exercise a disposable REM desktop candidate through its serial console."""
import argparse
import os
from pathlib import Path
import selectors
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]

class Console:
    def __init__(self, process, log):
        self.process = process
        self.log = log
        self.selector = selectors.DefaultSelector()
        self.selector.register(process.stdout, selectors.EVENT_READ)
        self.buffer = bytearray()

    def wait_for(self, token, timeout=40):
        token = token.encode()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if token in self.buffer:
                return True
            if self.process.poll() is not None:
                return False
            ready = self.selector.select(0.2)
            if not ready:
                continue
            chunk = os.read(self.process.stdout.fileno(), 8192)
            if not chunk:
                return False
            self.log.write(chunk)
            self.log.flush()
            self.buffer.extend(chunk)
            if len(self.buffer) > 4 * 1024 * 1024:
                del self.buffer[:2 * 1024 * 1024]
        return token in self.buffer

    def send(self, text):
        self.process.stdin.write(text.encode())
        self.process.stdin.flush()

    def command(self, command, marker, timeout=50):
        self.send(command + "\n")
        return self.wait_for(marker, timeout)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--qemu", type=Path, default=ROOT / "REM-FLIGHT-HUMAN-UAT-FULL/qemu-system-myemulator32")
    parser.add_argument("--kernel", type=Path, default=ROOT / "REM-FLIGHT-HUMAN-UAT-FULL/vmlinux")
    parser.add_argument("--password", default="rem0desktop")
    parser.add_argument("--work", type=Path, default=Path("/tmp/rem-linux-0.2-qualification"))
    args = parser.parse_args()
    image = args.image.resolve(strict=True)
    work = args.work.resolve()
    if not str(work).startswith("/tmp/"):
        raise SystemExit("qualification work directory must be under /tmp")
    if work.exists() and any(work.iterdir()):
        raise SystemExit(f"refusing to overwrite non-empty test directory: {work}")
    work.mkdir(parents=True, exist_ok=True)
    trial_image = work / "rootfs.ext4"
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", str(image), str(trial_image)], check=True)
    log_path = work / "serial.log"
    qemu = str(args.qemu.resolve(strict=True))
    kernel = str(args.kernel.resolve(strict=True))
    cmdline = ("console=ttyMY0,115200 earlycon=myemulator2,0xf0000000 "
               "root=/dev/myemu0 rw init=/sbin/init "
               "virtio_mmio.device=0x200@0xf0200000:5 ip=off")
    command = [qemu, "-M", "myemulator32", "-m", "128M", "-kernel", kernel,
               "-append", cmdline,
               "-drive", f"file={trial_image},format=raw,if=none,id=myemulator2-disk",
               "-nographic", "-monitor", "none", "-serial", "stdio",
               "-icount", "shift=0,sleep=off", "-no-reboot"]
    print("REM_QA|BOOT|START", flush=True)
    with log_path.open("wb") as log:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, bufsize=0)
        console = Console(process, log)
        try:
            for marker in ("REM_INIT|pid=1|REM Linux 0.2", "REM_STAGE|rcS|start",
                           "REM_INIT|getty|pid=", "REM Linux 0.2 login:"):
                if not console.wait_for(marker, 180):
                    print(f"REM_QA|BOOT|FAIL|missing {marker}", flush=True)
                    raise SystemExit(1)
                print(f"REM_QA|BOOT_MARKER|{marker}", flush=True)
            if console.wait_for("REM_INIT|proc-sys-dev|PASS", 1):
                print("REM_QA|MOUNTS|PASS", flush=True)
            elif console.wait_for("REM_INIT|proc-sys-dev|FAIL", 1):
                print("REM_QA|MOUNTS|FAIL", flush=True)
            else:
                print("REM_QA|MOUNTS|FAIL|marker missing", flush=True)
            if console.wait_for("REM_INIT|hostname|PASS", 1):
                print("REM_QA|HOSTNAME|PASS", flush=True)
            elif console.wait_for("REM_INIT|hostname|FAIL", 1):
                print("REM_QA|HOSTNAME|FAIL", flush=True)
            if console.wait_for("REM_NET|eth0=", 1):
                print("REM_QA|NETWORK|PASS", flush=True)
            elif console.wait_for("REM_NET|eth0-warning|", 1):
                print("REM_QA|NETWORK|FAIL", flush=True)
            console.send("dev\n")
            if not console.wait_for("Password:", 20):
                print("REM_QA|LOGIN|FAIL|password prompt missing", flush=True)
                raise SystemExit(1)
            console.send(args.password + "\n")
            if not console.wait_for("Welcome to REM Linux 0.2, dev.", 20):
                if console.wait_for("MYEMU_BAD_USER_FAULT", 1):
                    print("REM_QA|LOGIN|FAIL|kernel user fault while checking password hash", flush=True)
                else:
                    print("REM_QA|LOGIN|FAIL|authentication did not reach shell", flush=True)
                raise SystemExit(1)
            tests = [
                ("mounts", "test -r /proc/mounts && grep -q ' /proc proc ' /proc/mounts && grep -q ' /sys sysfs ' /proc/mounts && test -d /dev/pts && echo REM_QA|MOUNTS|PASS"),
                ("filesystem-tools", "printf 'alpha\\nbeta\\n' | grep -qx beta && printf 'alpha\\nbeta\\n' | sed -n '2p' | grep -qx beta && find /etc -name hostname | grep -qx /etc/hostname && echo REM_QA|FILESYSTEM_TOOLS|PASS"),
                ("archives", "mkdir -p /tmp/rem-qa && printf payload > /tmp/rem-qa/in && tar -cf /tmp/rem-qa/a.tar -C /tmp/rem-qa in && tar -xf /tmp/rem-qa/a.tar -C /tmp/rem-qa && gzip -c /tmp/rem-qa/in | gzip -dc > /tmp/rem-qa/g && bzip2 -c /tmp/rem-qa/in | bzip2 -dc > /tmp/rem-qa/b && xz -c /tmp/rem-qa/in | xz -dc > /tmp/rem-qa/x && diff /tmp/rem-qa/in /tmp/rem-qa/g && diff /tmp/rem-qa/in /tmp/rem-qa/b && diff /tmp/rem-qa/in /tmp/rem-qa/x && echo REM_QA|ARCHIVES|PASS"),
                ("chibicc", "printf '#include <stdio.h>\\nint main(void){puts(\\\"hello-rem\\\");return 0;}\\n' > /tmp/rem-qa/hello.c && chibicc /tmp/rem-qa/hello.c -o /tmp/rem-qa/hello && /tmp/rem-qa/hello | grep -qx hello-rem && echo REM_QA|CHIBICC|PASS"),
                ("assembler-linker", "chibicc -S /tmp/rem-qa/hello.c -o /tmp/rem-qa/hello.s && as /tmp/rem-qa/hello.s -o /tmp/rem-qa/hello.o && ld -r /tmp/rem-qa/hello.o -o /tmp/rem-qa/hello-rel.o && echo REM_QA|ASSEMBLER_LINKER|PASS"),
                ("samurai", "printf 'rule cc\\n  command = chibicc -c $in -o $out\\nbuild hi.o: cc /tmp/rem-qa/hello.c\\ndefault hi.o\\n' > /tmp/rem-qa/build.ninja && samu -f /tmp/rem-qa/build.ninja && test -f /tmp/rem-qa/hi.o && echo REM_QA|SAMURAI|PASS"),
                ("editor", "vim --version | grep -q 'VIM - Vi IMproved' && echo REM_QA|EDITOR|PASS"),
            ]
            for name, test in tests:
                if not console.command(test, f"REM_QA|{name.upper().replace('-', '_')}|PASS", 80):
                    print(f"REM_QA|{name}|FAIL", flush=True)
                    raise SystemExit(1)
                print(f"REM_QA|{name}|PASS", flush=True)
            if not console.command("/sbin/poweroff", "Power down", 60):
                print("REM_QA|POWEROFF|FAIL", flush=True)
                raise SystemExit(1)
            process.wait(timeout=20)
            print("REM_QA|POWEROFF|PASS", flush=True)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    check = subprocess.run(["e2fsck", "-fn", str(trial_image)], text=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (work / "e2fsck.log").write_text(check.stdout)
    if check.returncode:
        print("REM_QA|FILESYSTEM_CLEAN|FAIL", flush=True)
        raise SystemExit(1)
    print("REM_QA|FILESYSTEM_CLEAN|PASS", flush=True)
    print(f"REM_QA|LOG|{log_path}", flush=True)
    print("REM_QA|DONE|PASS", flush=True)

if __name__ == "__main__":
    main()
