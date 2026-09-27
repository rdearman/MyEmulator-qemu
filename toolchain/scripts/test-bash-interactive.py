#!/usr/bin/env python3
"""Exercise Bash interactive startup and bidirectional console input."""

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import tempfile
import time
from pathlib import Path

import pexpect


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fault_pc(text: str) -> str | None:
    match = re.search(r"\b(?:pc|PC)\s*[=:]\s*(0x[0-9a-fA-F]+|[0-9a-fA-F]{6,})", text)
    return match.group(1) if match else None


def stop_child(child: pexpect.spawn, owns_group: bool) -> None:
    try:
        (os.killpg if owns_group else os.kill)(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    deadline = time.monotonic() + 0.25
    while child.isalive() and time.monotonic() < deadline:
        time.sleep(0.02)
    if child.isalive():
        try:
            (os.killpg if owns_group else os.kill)(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    try:
        child.close(force=True)
    except (OSError, pexpect.ExceptionPexpect):
        pass


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rootfs", required=True, type=Path)
    parser.add_argument("--qemu", required=True, type=Path)
    parser.add_argument("--kernel", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("/tmp/rem-bash-interactive"))
    parser.add_argument("--memory", default="128M")
    parser.add_argument("--boot-timeout", type=int, default=45)
    parser.add_argument("--timeout", type=int, default=15,
                        help="timeout for the interactive Bash prompt and input checks")
    args = parser.parse_args()
    for artifact in (args.rootfs, args.qemu, args.kernel):
        if not artifact.is_file():
            parser.error(f"missing artifact: {artifact}")
    args.output.mkdir(parents=True, exist_ok=True)
    image = args.output / "rootfs.ext4"
    log_path = args.output / "console.log"
    report_path = args.output / "report.json"
    bash_tmp = args.output / "bash.elf"
    subprocess.run(["cp", "--sparse=always", "--reflink=auto", str(args.rootfs), str(image)], check=True)
    subprocess.run(["debugfs", "-R", f"dump /usr/bin/bash {bash_tmp}", str(image)],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    elf_text = subprocess.run(["readelf", "-h", "-p", ".comment", str(bash_tmp)],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              check=False).stdout
    report = {
        "status": "NOT RUN",
        "artifacts": {
            "qemu": str(args.qemu), "qemu_sha256": sha256(args.qemu),
            "kernel": str(args.kernel), "kernel_sha256": sha256(args.kernel),
            "rootfs": str(args.rootfs), "rootfs_sha256": sha256(args.rootfs),
            "bash_guest_path": "/usr/bin/bash", "bash_sha256": sha256(bash_tmp),
            "bash_elf_metadata": elf_text,
        },
        "configuration": {"machine": "myemulator32", "memory": args.memory,
                           "console": "nographic serial, monitor disabled"},
        "interactive_command": "PS1='REM_BASH_INTERACTIVE> ' /usr/bin/bash --noprofile --norc -i",
        "minimal_reproducer": "Start the command above from the booted BusyBox ash serial console; wait for REM_BASH_INTERACTIVE>.",
        "tests": [],
    }
    qemu_args = [str(args.qemu), "-M", "myemulator32", "-m", args.memory,
                 "-kernel", str(args.kernel), "-drive",
                 f"file={image},format=raw,if=none,id=myemulator2-disk",
                 "-nographic", "-monitor", "none", "-icount", "shift=0,sleep=off"]
    child = pexpect.spawn(qemu_args[0], qemu_args[1:], timeout=1,
                          encoding="utf-8", codec_errors="replace")
    owns_group = False
    try:
        os.setpgid(child.pid, child.pid)
        owns_group = True
    except (PermissionError, ProcessLookupError):
        pass
    try:
        with log_path.open("w") as log:
            child.logfile = log
            stage = "boot to parent shell"
            try:
                stage = "boot to parent shell"
                child.expect(r"# ", timeout=args.boot_timeout)
                child.sendline("stty -echo")
                child.expect(r"# ", timeout=8)
                stage = "Bash interactive startup and prompt"
                child.sendline("PS1='REM_BASH_INTERACTIVE> ' /usr/bin/bash --noprofile --norc -i")
                child.expect("REM_BASH_INTERACTIVE> ", timeout=args.timeout)
                report["tests"].append({"name": "interactive startup", "status": "PASS"})
                stage = "host input and guest response"
                child.sendline("printf 'REM_BASH_INTERACTIVE_INPUT_OK\\n'")
                child.expect("REM_BASH_INTERACTIVE_INPUT_OK", timeout=12)
                report["tests"].append({"name": "host input and guest response", "status": "PASS"})
                stage = "interactive exit returns to parent shell"
                child.sendline("exit")
                child.expect(r"# ", timeout=8)
                report["tests"].append({"name": "interactive exit returns to parent shell", "status": "PASS"})
                report["status"] = "PASS"
            except (pexpect.TIMEOUT, pexpect.EOF) as exc:
                log.flush()
                transcript = log_path.read_text(errors="replace")
                report["status"] = "FAIL"
                report["failure"] = str(exc)
                report["tests"].append({"name": stage, "status": "FAIL"})
                report["fault_pc"] = fault_pc(transcript)
                if "MYEMU_BAD_USER_FAULT" in transcript:
                    report["failure_layer"] = "userspace fault; causal subsystem unresolved"
                if "setpgid: Function not implemented" in transcript:
                    report["observed_syscall_failure"] = "setpgid returned ENOSYS in the tested tuple"
                    report["syscall_failure_layer"] = "syscall/kernel or libc ABI tuple mismatch"
                report["serial_log_tail"] = transcript[-6000:]
    finally:
        stop_child(child, owns_group)
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(report_path)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
