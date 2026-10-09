#!/usr/bin/env python3
"""Run the installed userland smoke suite from rc.local on a disposable image."""
import argparse
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--work", type=Path, default=Path("/tmp/rem-linux-0.2-package-smoke"))
    args = parser.parse_args()
    image = args.image.resolve(strict=True)
    work = args.work.resolve()
    if not str(work).startswith("/tmp/"):
        raise SystemExit("smoke work directory must be under /tmp")
    if work.exists() and any(work.iterdir()):
        raise SystemExit(f"refusing to overwrite non-empty test directory: {work}")
    work.mkdir(parents=True, exist_ok=True)
    trial = work / "rootfs.ext4"
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", str(image), str(trial)], check=True)
    hook = ROOT / "distribution/tests/in-guest-smoke.sh"
    commands = work / "debugfs.commands"
    commands.write_text("rm /etc/rc.local\nwrite " + str(hook) +
                        " /etc/rc.local\nset_inode_field /etc/rc.local mode 0100755\nquit\n")
    subprocess.run(["debugfs", "-w", "-f", str(commands), str(trial)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    before = subprocess.run(["e2fsck", "-fn", str(trial)], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    if before.returncode:
        (work / "e2fsck-before.log").write_text(before.stdout)
        raise SystemExit("REM_QA|PACKAGE_SMOKE|FAIL|test image dirty before boot")
    serial = work / "serial.log"
    qemu = ROOT / "REM-FLIGHT-HUMAN-UAT-FULL/qemu-system-myemulator32"
    kernel = ROOT / "REM-FLIGHT-HUMAN-UAT-FULL/vmlinux"
    cmdline = ("console=ttyMY0,115200 earlycon=myemulator2,0xf0000000 "
               "root=/dev/myemu0 rw init=/sbin/init "
               "virtio_mmio.device=0x200@0xf0200000:5 ip=off")
    command = [str(qemu), "-M", "myemulator32", "-m", "128M", "-kernel", str(kernel),
               "-append", cmdline,
               "-drive", f"file={trial},format=raw,if=none,id=myemulator2-disk",
               "-nographic", "-monitor", "none", "-serial", f"file:{serial}",
               "-icount", "shift=0,sleep=off", "-no-reboot"]
    print("REM_QA|PACKAGE_SMOKE|BOOT_START", flush=True)
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 240
    while time.monotonic() < deadline and process.poll() is None:
        data = serial.read_bytes() if serial.exists() else b""
        if b"REM_QA|PACKAGE_SMOKE|PASS" in data or b"REM_QA|PACKAGE_SMOKE|FAIL" in data:
            break
        if b"REM_QA|PACKAGE_SMOKE|FAIL" in data:
            break
        time.sleep(1)
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    (work / "qemu.stderr.log").write_bytes(process.stderr.read() if process.stderr else b"")
    data = serial.read_bytes() if serial.exists() else b""
    (work / "serial.txt").write_bytes(data)
    required = (b"REM_QA|PROC_SYS_DEV|PASS", b"REM_QA|PACKAGES_PRESENT|PASS",
                b"REM_QA|CORE_SEARCH|PASS", b"REM_QA|ARCHIVES|PASS",
                b"REM_QA|CHIBICC_AS_LD|PASS", b"REM_QA|SAMURAI|PASS",
                b"REM_QA|GREP|", b"REM_QA|SED|", b"REM_QA|FIND|",
                b"REM_QA|ARCHIVES|", b"REM_QA|CHIBICC_AS_LD|",
                b"REM_QA|SAMURAI|", b"REM_QA|EDITOR_CURL|",
                b"REM_QA|PACKAGE_SMOKE|", b"REM_SHUTDOWN|runlevel-stop",
                b"Power down")
    missing = [marker.decode() for marker in required if marker not in data]
    if missing:
        raise SystemExit("REM_QA|PACKAGE_SMOKE|FAIL|missing: " + ", ".join(missing))
    if process.returncode != 0:
        raise SystemExit(f"REM_QA|PACKAGE_SMOKE|FAIL|QEMU exit={process.returncode}")
    after = subprocess.run(["e2fsck", "-fn", str(trial)], stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
    (work / "e2fsck-after.log").write_text(after.stdout)
    if after.returncode:
        raise SystemExit("REM_QA|PACKAGE_SMOKE|FAIL|filesystem not clean after poweroff")
    if b"REM_QA|PACKAGE_SMOKE|PASS" in data:
        print("REM_QA|PACKAGE_SMOKE|PASS", flush=True)
    else:
        print("REM_QA|PACKAGE_SMOKE|PARTIAL|see serial log", flush=True)
    print("REM_QA|FILESYSTEM_CLEAN_AFTER_PACKAGE_SMOKE|PASS", flush=True)
    print(f"REM_QA|LOG|{serial}", flush=True)

if __name__ == "__main__":
    main()
