#!/usr/bin/env python3
"""Exercise installed REM userland from a disposable QEMU root shell."""
import argparse
import os
from pathlib import Path
import selectors
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]

def wait_for(process, selector, log, buffer, marker, timeout):
    marker = marker.encode()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if marker in buffer:
            return True
        if process.poll() is not None:
            return False
        for key, _ in selector.select(0.2):
            chunk = os.read(key.fd, 8192)
            if not chunk:
                return False
            log.write(chunk)
            log.flush()
            buffer.extend(chunk)
            if len(buffer) > 4 * 1024 * 1024:
                del buffer[:2 * 1024 * 1024]
    return marker in buffer

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--work", type=Path, default=Path("/tmp/rem-linux-0.2-userland-smoke"))
    args = parser.parse_args()
    image = args.image.resolve(strict=True)
    work = args.work.resolve()
    if not str(work).startswith("/tmp/"):
        raise SystemExit("test work directory must be under /tmp")
    if work.exists() and any(work.iterdir()):
        raise SystemExit(f"refusing to overwrite test directory: {work}")
    work.mkdir(parents=True, exist_ok=True)
    trial = work / "rootfs.ext4"
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", str(image), str(trial)], check=True)
    qemu = ROOT / "REM-FLIGHT-HUMAN-UAT-FULL/qemu-system-myemulator32"
    kernel = ROOT / "REM-FLIGHT-HUMAN-UAT-FULL/vmlinux"
    cmdline = ("console=ttyMY0,115200 earlycon=myemulator2,0xf0000000 "
               "root=/dev/myemu0 rw init=/usr/bin/bash "
               "virtio_mmio.device=0x200@0xf0200000:5 ip=off")
    command = [str(qemu), "-M", "myemulator32", "-m", "128M", "-kernel", str(kernel),
               "-append", cmdline,
               "-drive", f"file={trial},format=raw,if=none,id=myemulator2-disk",
               "-nographic", "-monitor", "none", "-serial", "stdio",
               "-icount", "shift=0,sleep=off", "-no-reboot"]
    print("REM_USERLAND|BOOT|START", flush=True)
    passed = False
    with (work / "serial.log").open("wb") as log:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, bufsize=0)
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        buffer = bytearray()
        try:
            root_shell = False
            for _ in range(180):
                if b"bash-5." in buffer:
                    root_shell = True
                    break
                if b"REM_INIT|pid=1|REM Linux 0.2" in buffer:
                    raise SystemExit("REM_USERLAND|BOOT|BLOCKED|kernel command line forces /sbin/init")
                if process.poll() is not None:
                    break
                wait_for(process, selector, log, buffer, "bash-5.", 1)
            if not root_shell:
                raise SystemExit("REM_USERLAND|BOOT|FAIL|root shell prompt missing")
            tests = [
                ("mounts", "mount -t proc proc /proc && mount -t sysfs sysfs /sys && mount -t devpts devpts /dev/pts && mount -t tmpfs tmpfs /tmp && test -d /proc/1 && test -d /sys/class && test -e /dev/pts/ptmx && echo REM_USERLAND|MOUNTS|PASS"),
                ("filesystem-tools", "printf 'alpha\\nbeta\\n' | grep -qx beta && printf 'alpha\\nbeta\\n' | sed -n '2p' | grep -qx beta && find /etc -name hostname | grep -qx /etc/hostname && echo REM_USERLAND|FILESYSTEM|PASS"),
                ("archives", "mkdir -p /tmp/rem-qa && printf payload > /tmp/rem-qa/in && tar -cf /tmp/rem-qa/a.tar -C /tmp/rem-qa in && tar -xf /tmp/rem-qa/a.tar -C /tmp/rem-qa && gzip -c /tmp/rem-qa/in | gzip -dc > /tmp/rem-qa/g && bzip2 -c /tmp/rem-qa/in | bzip2 -dc > /tmp/rem-qa/b && xz -c /tmp/rem-qa/in | xz -dc > /tmp/rem-qa/x && diff /tmp/rem-qa/in /tmp/rem-qa/g && diff /tmp/rem-qa/in /tmp/rem-qa/b && diff /tmp/rem-qa/in /tmp/rem-qa/x && echo REM_USERLAND|ARCHIVES|PASS"),
                ("chibicc", "printf '#include <stdio.h>\\nint main(void){puts(\\\"hello-rem\\\");return 0;}\\n' > /tmp/rem-qa/hello.c && chibicc /tmp/rem-qa/hello.c -o /tmp/rem-qa/hello && /tmp/rem-qa/hello | grep -qx hello-rem && echo REM_USERLAND|CHIBICC|PASS"),
                ("assembler-linker", "chibicc -S /tmp/rem-qa/hello.c -o /tmp/rem-qa/hello.s && as /tmp/rem-qa/hello.s -o /tmp/rem-qa/hello.o && ld -r /tmp/rem-qa/hello.o -o /tmp/rem-qa/hello-rel.o && echo REM_USERLAND|BINUTILS|PASS"),
                ("samurai", "printf 'rule cc\\n  command = chibicc -c $in -o $out\\nbuild hi.o: cc /tmp/rem-qa/hello.c\\ndefault hi.o\\n' > /tmp/rem-qa/build.ninja && samu -f /tmp/rem-qa/build.ninja && test -f /tmp/rem-qa/hi.o && echo REM_USERLAND|SAMURAI|PASS"),
                ("editor", "vim --version | grep -q 'VIM - Vi IMproved' && echo REM_USERLAND|EDITOR|PASS"),
            ]
            for name, cmd in tests:
                process.stdin.write((cmd + "\n").encode())
                process.stdin.flush()
                marker = "REM_USERLAND|" + ("FILESYSTEM" if name == "filesystem-tools" else
                    "BINUTILS" if name == "assembler-linker" else name.upper()) + "|PASS"
                if not wait_for(process, selector, log, buffer, marker, 90):
                    raise SystemExit(f"REM_USERLAND|{name}|FAIL")
                print(f"REM_USERLAND|{name}|PASS", flush=True)
            process.stdin.write(b"/sbin/poweroff\n")
            process.stdin.flush()
            if not wait_for(process, selector, log, buffer, "Power down", 90):
                raise SystemExit("REM_USERLAND|POWEROFF|FAIL")
            process.wait(timeout=20)
            passed = True
            print("REM_USERLAND|POWEROFF|PASS", flush=True)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    if not passed:
        raise SystemExit(1)
    fsck = subprocess.run(["e2fsck", "-fn", str(trial)], stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True)
    (work / "e2fsck.log").write_text(fsck.stdout)
    if fsck.returncode:
        print("REM_USERLAND|FILESYSTEM_CLEAN|FAIL", flush=True)
        raise SystemExit(1)
    print("REM_USERLAND|FILESYSTEM_CLEAN|PASS", flush=True)

if __name__ == "__main__":
    main()
