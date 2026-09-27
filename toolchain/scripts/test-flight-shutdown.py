#!/usr/bin/env python3
"""Verify the REM PID-1 shell supervisor and sync/restart persistence."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pexpect
import shutil
import signal
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def stop(child: pexpect.spawn, own_group: bool) -> None:
    killer = os.killpg if own_group else os.kill
    try:
        killer(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    deadline = time.monotonic() + 1
    reaped = False
    while time.monotonic() < deadline:
        try:
            waited, _ = os.waitpid(child.pid, os.WNOHANG)
            if waited == child.pid:
                reaped = True
                break
        except ChildProcessError:
            reaped = True
            break
        time.sleep(0.02)
    if not reaped:
        try:
            killer(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        try:
            os.waitpid(child.pid, 0)
        except ChildProcessError:
            pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rootfs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--qemu", type=Path, default=ROOT / ".qemu-build/qemu-system-myemulator32")
    parser.add_argument("--kernel", type=Path, default=ROOT / ".rem-flight-build-20260924/linux-ppoll-build/vmlinux")
    parser.add_argument("--memory", default="16M")
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()
    for path in (args.rootfs, args.qemu, args.kernel):
        if not path.is_file():
            parser.error(f"missing file: {path}")
    args.output.mkdir(parents=True, exist_ok=False)
    image = args.output / "rootfs.ext4"
    # Preserve sparse 10 GiB deployment images in the disposable test copy.
    subprocess.run(["cp", "--sparse=always", "--reflink=auto", str(args.rootfs), str(image)],
                   check=True)
    log_path = args.output / "console.log"
    report_path = args.output / "result.json"
    qemu_cmd = [
        str(args.qemu.resolve()), "-M", "myemulator32", "-m", args.memory,
        "-kernel", str(args.kernel.resolve()), "-drive",
        f"file={image.resolve()},format=raw,if=none,id=myemulator2-disk",
        "-nographic", "-monitor", "none", "-serial", "stdio",
        "-icount", "shift=0,sleep=off",
    ]
    report = {
        "status": "NOT TESTED", "qemu": qemu_cmd[0],
        "qemu_sha256": sha256(args.qemu), "kernel": qemu_cmd[6],
        "kernel_sha256": sha256(args.kernel), "rootfs": str(image.resolve()),
        "rootfs_seed_sha256": sha256(args.rootfs), "console_log": str(log_path),
        "steps": [],
    }
    child: pexpect.spawn | None = None
    own_group = False
    started = time.monotonic()

    def boot(log) -> tuple[pexpect.spawn, bool]:
        proc = pexpect.spawn(qemu_cmd[0], qemu_cmd[1:], timeout=args.timeout,
                             encoding="utf-8", codec_errors="replace")
        try:
            os.setpgid(proc.pid, proc.pid)
            group = True
        except (PermissionError, ProcessLookupError):
            group = False
        proc.logfile = log
        proc.expect(r"(?:\r?\n|\x1b\[\?1049l)# ", timeout=args.timeout)
        return proc, group

    def run_command(proc: pexpect.spawn, line: str, marker: str,
                    expected: str | None = None) -> str:
        proc.sendline(f"{line}; echo {marker}:$?")
        proc.expect(rf"{marker}:(\d+)", timeout=args.timeout)
        rc = proc.match.group(1)
        output = proc.before
        if rc != "0":
            raise RuntimeError(f"{line} returned {rc}")
        if expected is not None and expected not in output:
            raise RuntimeError(f"{line}: expected output absent: {expected}")
        proc.expect(r"(?:\r?\n|\[\?1049l)# ", timeout=args.timeout)
        report["steps"].append(f"{marker}=0")
        return output

    try:
        with log_path.open("w") as log:
            log.write("=== boot 1: write, wait for automatic sync, stop QEMU ===\n")
            log.flush()
            child, own_group = boot(log)
            run_command(child, "echo REM_PERSISTENCE_CONFIRMED > /root/sync-test.txt",
                        "WRITE_RC")
            run_command(child, "cat /root/sync-test.txt", "READ_BEFORE_WAIT_RC",
                        "REM_PERSISTENCE_CONFIRMED")
            run_command(child, "/bin/busybox sleep 4", "WAIT_FOR_AUTO_SYNC_RC")
            run_command(child, "cat /root/sync-test.txt", "READ_AFTER_WAIT_RC",
                        "REM_PERSISTENCE_CONFIRMED")
            stop(child, own_group)
            child = None

            log.write("\n=== boot 2: verify persisted file ===\n")
            log.flush()
            child, own_group = boot(log)
            run_command(child, "cat /root/sync-test.txt", "PERSIST_RC",
                        "REM_PERSISTENCE_CONFIRMED")
            child.sendline("exit")
            child.expect("REM shell exited", timeout=args.timeout)
            child.expect(r"(?:\r?\n|\x1b\[\?1049l)# ", timeout=args.timeout)
            report["steps"].append("exit returned to supervised shell; no PID-1 panic")
            transcript = log_path.read_text(errors="replace")
            if "Kernel panic" in transcript or "MYEMU_BAD_USER_FAULT" in transcript:
                raise RuntimeError("kernel fault/panic found in boot transcript")
            report["status"] = "PASS"
    except (pexpect.TIMEOUT, pexpect.EOF, RuntimeError, OSError) as exc:
        report["status"] = "FAIL"
        report["error"] = str(exc)
    finally:
        if child is not None:
            stop(child, own_group)
        report["elapsed_seconds"] = round(time.monotonic() - started, 3)
        report["rootfs_sha256_final"] = sha256(image)
        report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
