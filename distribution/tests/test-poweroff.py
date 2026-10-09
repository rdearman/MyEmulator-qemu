#!/usr/bin/env python3
"""Test the REM shutdown path on a disposable image via its startup hook."""
import argparse
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--work", type=Path, default=Path("/tmp/rem-linux-0.2-poweroff-test"))
    parser.add_argument("--action", choices=("poweroff", "reboot"), default="poweroff")
    args = parser.parse_args()
    image = args.image.resolve(strict=True)
    work = args.work.resolve()
    if not str(work).startswith("/tmp/"):
        raise SystemExit("poweroff test directory must be under /tmp")
    if work.exists() and any(work.iterdir()):
        raise SystemExit(f"refusing to overwrite test directory: {work}")
    work.mkdir(parents=True, exist_ok=True)
    trial = work / "rootfs.ext4"
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", str(image), str(trial)], check=True)
    hook = work / "rc.local"
    if args.action == "reboot":
        hook.write_text("#!/bin/sh\n"
                        "if [ ! -f /etc/.rem-reboot-once ]; then\n"
                        "  : > /etc/.rem-reboot-once\n"
                        "  echo 'REM_QA|REBOOT_HOOK|START'\n"
                        "  /sbin/reboot\n"
                        "fi\n"
                        "echo 'REM_QA|REBOOT|SECOND_BOOT'\n"
                        "/sbin/poweroff\n")
    else:
        hook.write_text("#!/bin/sh\necho 'REM_QA|POWEROFF_HOOK|START'\n/sbin/poweroff\n")
    hook.chmod(0o755)
    commands = work / "debugfs.commands"
    commands.write_text("rm /etc/rc.local\nwrite " + str(hook) +
                        " /etc/rc.local\nset_inode_field /etc/rc.local mode 0100755\nquit\n")
    subprocess.run(["debugfs", "-w", "-f", str(commands), str(trial)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    fsck_before = subprocess.run(["e2fsck", "-fn", str(trial)], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True)
    if fsck_before.returncode:
        (work / "e2fsck-before.log").write_text(fsck_before.stdout)
        raise SystemExit("REM_QA|POWEROFF|FAIL|test image dirty before boot")
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
               "-icount", "shift=0,sleep=off"]
    if args.action == "poweroff":
        command.append("-no-reboot")
    print(f"REM_QA|{args.action.upper()}|BOOT_START", flush=True)
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 180
    reboot_requested = False
    second_boot = False
    while time.monotonic() < deadline and process.poll() is None:
        if args.action == "reboot" and serial.exists():
            log_data = serial.read_bytes()
            if b"REM_QA|REBOOT|SECOND_BOOT" in log_data:
                second_boot = True
            if b"MyEmulator2 restart requested" in log_data:
                reboot_requested = True
        time.sleep(1)
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        raise SystemExit(f"REM_QA|{args.action.upper()}|FAIL|QEMU did not stop")
    data = serial.read_bytes() if serial.exists() else b""
    (work / "qemu.stderr.log").write_bytes(process.stderr.read() if process.stderr else b"")
    if (f"REM_QA|{args.action.upper()}_HOOK|START".encode() not in data or
            b"REM_SHUTDOWN|runlevel-stop" not in data):
        raise SystemExit(f"REM_QA|{args.action.upper()}|FAIL|shutdown hook did not complete")
    if args.action == "reboot" and (not reboot_requested or not second_boot):
        raise SystemExit("REM_QA|REBOOT|FAIL|guest did not complete a second boot")
    if b"runlevel 0: local stop failed" in data or b"runlevel 0: network stop failed" in data:
        raise SystemExit(f"REM_QA|{args.action.upper()}|FAIL|runlevel stop service failed")
    fsck = subprocess.run(["e2fsck", "-fn", str(trial)], stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True)
    (work / "e2fsck-after.log").write_text(fsck.stdout)
    if fsck.returncode:
        raise SystemExit(f"REM_QA|{args.action.upper()}|FAIL|filesystem not clean after shutdown")
    print(f"REM_QA|{args.action.upper()}|PASS", flush=True)
    print(f"REM_QA|FILESYSTEM_CLEAN_AFTER_{args.action.upper()}|PASS", flush=True)

if __name__ == "__main__":
    main()
