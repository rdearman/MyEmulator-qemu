#!/usr/bin/env python3
"""Run the REM userspace regression script in one disposable QEMU guest."""

import argparse
import hashlib
import json
import os
import shlex
import shutil
import signal
import subprocess
import tempfile
import time
from pathlib import Path

import pexpect

ROOT = Path(__file__).resolve().parents[2]
GUEST = ROOT / "toolchain/userspace/guest-regression.sh"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inject(image, host_script):
    command = ["debugfs", "-w", "-R", f"write {host_script} /tmp/rem-userspace-regression.sh", str(image)]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def classify(rc):
    if rc == 0:
        return "PASS"
    if rc == 127:
        return "SKIP"
    if rc >= 128:
        return "CRASH"
    return "FAIL"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rootfs", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("/tmp/rem-userspace-regression"))
    parser.add_argument("--qemu", type=Path, default=ROOT / ".qemu-build/qemu-system-myemulator32")
    parser.add_argument("--kernel", type=Path, default=ROOT / ".linux-build/build/vmlinux")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--test")
    parser.add_argument("--group")
    parser.add_argument("--qemu-trace", action="store_true")
    args = parser.parse_args()
    if not args.rootfs.is_file():
        parser.error(f"missing rootfs: {args.rootfs}")
    if not args.qemu.is_file() or not args.kernel.is_file():
        parser.error("QEMU or kernel artifact is missing")
    args.output.mkdir(parents=True, exist_ok=True)
    image = args.output / "rootfs.ext4"
    shutil.copyfile(args.rootfs, image)
    inject(image, GUEST)
    qemu_log = args.output / "console.log"
    qemu_args = [str(args.qemu), "-M", "myemulator32", "-m", "16M", "-kernel", str(args.kernel),
                 "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk", "-nographic",
                 "-monitor", "none", "-icount", "shift=0,sleep=off"]
    if args.qemu_trace:
        qemu_args += ["-d", "guest_errors,int", "-D", str(args.output / "qemu-trace.log")]
    started = time.monotonic()
    child = pexpect.spawn(qemu_args[0], qemu_args[1:], timeout=1, encoding="utf-8", codec_errors="replace")
    report = {"artifacts": {"qemu": str(args.qemu), "kernel": str(args.kernel), "rootfs": str(args.rootfs),
                             "qemu_sha256": sha256(args.qemu), "kernel_sha256": sha256(args.kernel),
                             "rootfs_sha256": sha256(args.rootfs)}, "tests": [], "status": "BOOT_FAIL"}
    try:
        with qemu_log.open("w") as log:
            child.logfile = log
            child.expect(r"# ", timeout=args.timeout)
            prefix = ""
            if args.test:
                prefix += f"REMTEST_ONLY={shlex.quote(args.test)} "
            if args.group:
                prefix += f"REMTEST_GROUP={shlex.quote(args.group)} "
            child.sendline(prefix + "sh /tmp/rem-userspace-regression.sh")
            child.expect("REMTEST_COMPLETE", timeout=args.timeout)
            report["status"] = "COMPLETED"
            log.flush()
            text = qemu_log.read_text(errors="replace")
            begin = {}
            for line in text.splitlines():
                if line.startswith("REMTEST_BEGIN|"):
                    _, name, path = line.split("|", 2)
                    begin[name] = {"name": name, "path": path, "started": time.monotonic()}
                elif line.startswith("REMTEST_END|"):
                    _, name, raw_rc = line.split("|", 2)
                    item = begin.pop(name, {"name": name, "path": ""})
                    rc = int(raw_rc)
                    item.update({"status": classify(rc), "exit_status": rc, "duration_seconds": 0})
                    report["tests"].append(item)
            report["tests"].sort(key=lambda item: item["name"])
    except pexpect.TIMEOUT:
        report["status"] = "TIMEOUT"
    except pexpect.EOF:
        report["status"] = "QEMU_EXITED"
    finally:
        try:
            if child.isalive():
                child.kill(signal.SIGTERM)
                time.sleep(0.2)
            if child.isalive():
                child.kill(signal.SIGKILL)
        finally:
            try:
                child.close(force=True)
            except Exception:
                pass
    report["duration_seconds"] = time.monotonic() - started
    report["summary"] = {status: sum(item.get("status") == status for item in report["tests"])
                         for status in ("PASS", "FAIL", "CRASH", "TIMEOUT", "SKIP", "NOT TESTED")}
    (args.output / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    (args.output / "summary.txt").write_text("\n".join([f"overall: {report['status']}"] +
        [f"{item['status']:7} {item['name']} {item['path']} rc={item.get('exit_status', '?')}" for item in report["tests"]]) + "\n")
    print(args.output / "report.json")
    return 0 if report["status"] == "COMPLETED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
