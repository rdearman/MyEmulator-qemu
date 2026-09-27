#!/usr/bin/env python3
"""Boot-test the documented REM BusyBox/editor/native-tools workflow."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pexpect
import shutil
import signal
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
    shutil.copyfile(args.rootfs, image)
    log_path = args.output / "console.log"
    result_path = args.output / "result.json"
    qemu = str(args.qemu.resolve())
    kernel = str(args.kernel.resolve())
    image_abs = str(image.resolve())
    qemu_args = [
        qemu, "-M", "myemulator32", "-m", args.memory, "-kernel", kernel,
        "-drive", f"file={image_abs},format=raw,if=none,id=myemulator2-disk",
        "-nographic", "-monitor", "none", "-serial", "stdio",
        "-icount", "shift=0,sleep=off",
    ]
    result = {
        "status": "NOT TESTED", "qemu": qemu, "qemu_sha256": sha256(args.qemu),
        "kernel": kernel, "kernel_sha256": sha256(args.kernel),
        "rootfs": image_abs, "rootfs_seed_sha256": sha256(args.rootfs),
        "memory": args.memory, "console_log": str(log_path), "steps": [],
    }
    child = pexpect.spawn(qemu, qemu_args[1:], timeout=1,
                          encoding="utf-8", codec_errors="replace")
    try:
        os.setpgid(child.pid, child.pid)
        own_group = True
    except (PermissionError, ProcessLookupError):
        own_group = False
    deadline = time.monotonic() + args.timeout

    def expect(pattern: str) -> None:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise pexpect.TIMEOUT("overall flight acceptance timeout")
        child.expect(pattern, timeout=remaining)

    def prompt() -> None:
        expect(r"(?:\r?\n|\x1b\[\?1049l)# ")

    def command(line: str, marker: str, expected: str | None = None) -> str:
        child.sendline(f"{line}; echo {marker}:$?")
        expect(rf"{marker}:(\d+)")
        status = child.match.group(1)
        output = child.before
        if status != "0":
            raise RuntimeError(f"{marker}: command exited {status}: {line}")
        if expected is not None and expected not in output:
            raise RuntimeError(f"{marker}: expected output absent: {expected}")
        prompt()
        result["steps"].append(marker + "=0")
        return output

    started = time.monotonic()
    try:
        with log_path.open("w") as log:
            child.logfile = log
            prompt()
            command('echo "PATH=$PATH"', "PROFILE_RC", "/root/native/bin")
            command("which ls cat pwd echo rem-as rem-ld rem-run", "WHICH_RC", "/bin/ls")
            command("pwd", "PWD_RC", "/")
            command("ls /", "LS_RC", "root")
            command("cat /root/README.md", "CAT_RC", "Edit, assemble, link, run")
            command("cat /root/docs/README.md", "DOCS_RC", "architecture/")
            command("rem-help", "HELP_RC", "programming/")
            command('echo NORMAL_ECHO_OK', "ECHO_RC", "NORMAL_ECHO_OK")
            command("/bin/echo APPLET_ECHO_OK", "BIN_ECHO_RC", "APPLET_ECHO_OK")

            child.sendline("cd /root; vi hello.S")
            expect(r"_start:")
            expect(r"\d+/\d+\s+\d+%")
            time.sleep(0.3)
            child.send("G")
            time.sleep(0.15)
            child.send("o")
            time.sleep(0.15)
            for char in "# FLIGHT_IMAGE_EDIT_OK":
                child.send(char)
                time.sleep(0.025)
            child.send("\x1b")
            time.sleep(0.2)
            child.send("Z")
            time.sleep(0.15)
            child.send("Z")
            prompt()
            result["steps"].append("vi interactive edit/save passed")
            command("cat /root/hello.S", "EDIT_RC", "FLIGHT_IMAGE_EDIT_OK")

            command("cd /root; /bin/busybox rm -f hello.o hello", "CLEAN_TOOLS_RC")
            command("cd /root; rem-as hello.S", "AS_RC")
            command("cd /root; rem-ld hello.o", "LD_RC")
            command("cd /root; rem-run hello", "RUN_RC", "REM NATIVE LINK PASS")
            command("sync", "SYNC_RC")
            transcript = log_path.read_text(errors="replace")
            fatal = (
                "Segmentation fault", "Bus error", "MYEMU_BAD_USER_FAULT",
                "Kernel panic", "EXT4-fs error", "vi: poll: Function not implemented",
            )
            found = [marker for marker in fatal if marker in transcript]
            if found:
                raise RuntimeError("fatal guest diagnostics: " + ", ".join(found))
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
        end = time.monotonic() + 1
        reaped = False
        while time.monotonic() < end:
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
        result["status_json"] = str(result_path)
        result_path.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
