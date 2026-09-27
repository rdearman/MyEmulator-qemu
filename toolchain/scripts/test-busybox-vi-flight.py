#!/usr/bin/env python3
"""Interactive BusyBox vi -> native REM assembler/linker acceptance test."""

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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rootfs", type=Path, required=True, help="seed ext4 image")
    parser.add_argument("--output", type=Path, required=True, help="new disposable output directory")
    parser.add_argument("--qemu", type=Path, default=ROOT / ".qemu-build/qemu-system-myemulator32")
    parser.add_argument("--kernel", type=Path, default=ROOT / ".rem-flight-build-20260924/linux-tlbfix-build/vmlinux")
    parser.add_argument("--memory", default="16M")
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    for path in (args.rootfs, args.qemu, args.kernel):
        if not path.is_file():
            parser.error(f"missing file: {path}")
    args.output.mkdir(parents=True, exist_ok=False)
    image = args.output / "rootfs.ext4"
    shutil.copyfile(args.rootfs, image)
    log_path = args.output / "console.log"
    result_path = args.output / "result.json"
    qemu_args = [
        str(args.qemu), "-M", "myemulator32", "-m", args.memory,
        "-kernel", str(args.kernel), "-drive",
        f"file={image},format=raw,if=none,id=myemulator2-disk",
        "-nographic", "-monitor", "none", "-serial", "stdio",
        "-icount", "shift=0,sleep=off",
    ]
    result = {
        "status": "NOT TESTED",
        "rootfs": str(image),
        "rootfs_sha256_seed": sha256(args.rootfs),
        "qemu": str(args.qemu),
        "qemu_sha256": sha256(args.qemu),
        "kernel": str(args.kernel),
        "kernel_sha256": sha256(args.kernel),
        "memory": args.memory,
        "log": str(log_path),
        "steps": [],
    }
    child = pexpect.spawn(qemu_args[0], qemu_args[1:], timeout=1,
                          encoding="utf-8", codec_errors="replace")
    try:
        os.setpgid(child.pid, child.pid)
        own_group = True
    except (PermissionError, ProcessLookupError):
        own_group = False
    started = time.monotonic()
    deadline = started + args.timeout

    def expect(pattern: str) -> None:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise pexpect.TIMEOUT("overall test timeout")
        child.expect(pattern, timeout=remaining)

    def expect_shell_prompt() -> None:
        # vi exits its alternate screen and prints the prompt without a
        # preceding newline; ordinary command output usually has one.
        expect(r"(?:\r?\n|\x1b\[\?1049l)# ")

    try:
        with log_path.open("w") as log:
            child.logfile = log
            expect_shell_prompt()
            child.sendline("export TERM=linux; /bin/vi /root/hello.S")
            expect(r"_start:")
            # The buffer is painted before vi finishes terminal setup.  Wait
            # for its status line too, so keystrokes are not consumed by the
            # shell's canonical input mode during startup.
            expect(r"1/17\s+\d+%")
            time.sleep(0.3)
            result["steps"].append("BusyBox vi opened the assembly buffer")

            # Append a syntactically valid assembler comment using vi's
            # ordinary-mode commands; ZZ saves and exits without colon support.
            child.send("G")
            time.sleep(0.15)
            child.send("o")
            time.sleep(0.15)
            for char in "# EDITED_WITH_BUSYBOX_VI":
                child.send(char)
                time.sleep(0.025)
            child.send("\x1b")
            time.sleep(0.2)
            child.send("Z")
            time.sleep(0.15)
            child.send("Z")
            expect_shell_prompt()
            result["steps"].append("inserted text and exited with ZZ")

            child.sendline("/bin/cat /root/hello.S; echo EDIT_RC:$?")
            expect(r"EDIT_RC:(\d+)")
            output = child.before
            if child.match.group(1) != "0" or "EDITED_WITH_BUSYBOX_VI" not in output:
                raise RuntimeError("saved file does not contain the inserted marker")
            expect_shell_prompt()
            result["steps"].append("saved contents verified with cat")

            child.sendline("/bin/vi /root/hello.S")
            expect(r"EDITED_WITH_BUSYBOX_VI")
            result["steps"].append("reopened file and verified saved marker")
            child.send("Z")
            time.sleep(0.15)
            child.send("Z")
            expect_shell_prompt()

            for command, marker in (
                ("/root/native/bin/as -o /root/hello.o /root/hello.S", "AS_RC"),
                ("/root/native/bin/ld -T /root/native/lib/ldscripts/elf32myemulator2.x -o /root/hello /root/hello.o", "LD_RC"),
            ):
                child.sendline(f"{command}; echo {marker}:$?")
                expect(rf"{marker}:(\d+)")
                if child.match.group(1) != "0":
                    raise RuntimeError(f"{marker} returned {child.match.group(1)}")
                expect_shell_prompt()
                result["steps"].append(f"{marker}=0")

            child.sendline("/root/hello; echo RUN_RC:$?")
            expect(r"REM NATIVE LINK PASS")
            expect(r"RUN_RC:(\d+)")
            if child.match.group(1) != "0":
                raise RuntimeError(f"program exited {child.match.group(1)}")
            expect_shell_prompt()
            result["steps"].append("edited program output and exit status verified")
            child.sendline("sync; echo SYNC_RC:$?")
            expect(r"SYNC_RC:(\d+)")
            if child.match.group(1) != "0":
                raise RuntimeError(f"sync exited {child.match.group(1)}")
            expect_shell_prompt()
            result["steps"].append("sync returned 0 after editing and running")
            transcript = log_path.read_text(errors="replace")
            if any(marker in transcript for marker in (
                "Segmentation fault", "Bus error", "MYEMU_BAD_USER_FAULT",
                "Kernel panic", "EXT4-fs error", "vi: poll: Function not implemented",
            )):
                raise RuntimeError("guest transcript contains a fatal guest/kernel diagnostic")
            result["status"] = "PASS"
    except pexpect.TIMEOUT as exc:
        result["status"] = "TIMEOUT"
        result["error"] = str(exc)
    except (pexpect.EOF, RuntimeError, OSError) as exc:
        result["status"] = "FAIL"
        result["error"] = str(exc)
    finally:
        killer = os.killpg if own_group else os.kill
        try:
            killer(child.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        wait_deadline = time.monotonic() + 1.0
        reaped = False
        while time.monotonic() < wait_deadline:
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
        result["elapsed_seconds"] = round(time.monotonic() - started, 3)
        result["rootfs_sha256_final"] = sha256(image)
        result["console_log"] = str(log_path)
        result["status_json"] = str(result_path)
        result_path.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
