#!/usr/bin/env python3
"""Run the native REM Vim interactive acceptance test in an isolated guest.

The existing userspace runner deliberately remains non-interactive and keeps
its 26 test definitions unchanged.  This runner is test 27: it drives Vim
through the guest serial terminal, verifies a write, and records the complete
console transcript even when Vim or the guest fails.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import time
from pathlib import Path

import pexpect


ROOT = Path(__file__).resolve().parents[2]
PROMPT = re.compile(r"\r?\n# ")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def stop(child: pexpect.spawn, own_group: bool) -> None:
    try:
        killer = os.killpg if own_group else os.kill
        killer(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    deadline = time.monotonic() + 0.5
    while child.isalive() and time.monotonic() < deadline:
        time.sleep(0.02)
    if child.isalive():
        try:
            killer = os.killpg if own_group else os.kill
            killer(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    # pexpect.close(force=True) can wait on a QEMU child whose guest has
    # already entered a kernel panic.  The direct wait below is bounded and
    # leaves no QEMU descendant behind without making result recording depend
    # on pexpect's EOF handling.
    try:
        wait_deadline = time.monotonic() + 1.0
        while time.monotonic() < wait_deadline:
            waited, _ = os.waitpid(child.pid, os.WNOHANG)
            if waited == child.pid:
                return
            time.sleep(0.02)
        os.waitpid(child.pid, os.WNOHANG)
    except ChildProcessError:
        pass


def sparse_copy(source: Path, destination: Path) -> None:
    """Copy a disk image without expanding sparse ext4 free space."""
    subprocess.run(["cp", "--sparse=always", "--reflink=auto",
                    str(source), str(destination)], check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rootfs", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("/tmp/rem-vim-regression"))
    parser.add_argument("--qemu", type=Path, default=ROOT / ".qemu-build/qemu-system-myemulator32")
    parser.add_argument("--kernel", type=Path, default=ROOT / ".linux-build/build/vmlinux")
    parser.add_argument("--timeout", type=int, default=45)
    parser.add_argument("--memory", default="16M")
    parser.add_argument("--term", default="linux",
                        help="guest terminfo terminal name (default: linux)")
    parser.add_argument("--qemu-log-items",
                        help="optional QEMU trace items for focused diagnostics")
    parser.add_argument("--qemu-log-filter",
                        help="optional QEMU address filter, e.g. 0x02000000..0x02000040")
    parser.add_argument("--batch-probe", action="store_true",
                        help="also run the separate Ex-mode smoke test")
    args = parser.parse_args()
    for artifact in (args.rootfs, args.qemu, args.kernel):
        if not artifact.is_file():
            parser.error(f"missing artifact: {artifact}")

    args.output.mkdir(parents=True, exist_ok=True)
    image = args.output / "rootfs.ext4"
    log_path = args.output / "console.log"
    result_path = args.output / "result.json"
    sparse_copy(args.rootfs, image)

    qemu_args = [
        str(args.qemu), "-M", "myemulator32", "-m", args.memory,
        "-kernel", str(args.kernel),
        "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk",
        "-nographic", "-monitor", "none", "-serial", "stdio",
        "-icount", "shift=0,sleep=off",
    ]
    if args.qemu_log_items:
        qemu_args += ["-d", args.qemu_log_items,
                      "-D", str(args.output / "qemu-trace.log")]
        if args.qemu_log_filter:
            qemu_args += ["-dfilter", args.qemu_log_filter]
    child = pexpect.spawn(qemu_args[0], qemu_args[1:], timeout=1,
                          encoding="utf-8", codec_errors="replace")
    own_group = False
    try:
        os.setpgid(child.pid, child.pid)
        own_group = True
    except (PermissionError, ProcessLookupError):
        pass

    status = "NOT TESTED"
    detail = ""
    started = time.monotonic()
    deadline = started + args.timeout

    def remaining() -> float:
        value = deadline - time.monotonic()
        if value <= 0:
            raise pexpect.TIMEOUT("Vim acceptance deadline expired")
        return value

    with log_path.open("w") as log:
        child.logfile = log
        try:
            child.expect(PROMPT, timeout=remaining())
            child.sendline(f"export TERM={args.term}")
            child.expect(PROMPT, timeout=min(5, remaining()))
            child.sendline("/usr/bin/vim --version; echo VIM_VERSION_RC:$?")
            child.expect(r"VIM - Vi IMproved 9\.1", timeout=remaining())
            child.expect(r"VIM_VERSION_RC:(\d+)", timeout=remaining())
            if child.match.group(1) != "0":
                raise RuntimeError("vim --version returned non-zero")
            child.expect(PROMPT, timeout=remaining())

            if args.batch_probe:
                child.sendline(
                    '/usr/bin/vim -u NONE -i NONE -n -es '
                    '-c "call append(0, \'REM_VIM_BATCH\')" '
                    '-c "wq" /root/vim-batch.txt; echo VIM_BATCH_RC:$?'
                )
                child.expect(r"VIM_BATCH_RC:(\d+)", timeout=remaining())
                if child.match.group(1) != "0":
                    raise RuntimeError(f"Vim batch edit/save exited {child.match.group(1)}")
                child.expect(PROMPT, timeout=remaining())
                child.sendline("/bin/cat /root/vim-batch.txt")
                child.expect("REM_VIM_BATCH", timeout=remaining())
                child.expect(PROMPT, timeout=remaining())

            edit_command = (
                f"/usr/bin/vim -V15/root/vim-verbose.log -u NONE -i NONE -n "
                f"-T {args.term} /root/hello.S; "
                "echo VIM_EDIT_RC:$?"
            )
            child.sendline(edit_command)
            # Do not race Vim's startup by sending keys after an arbitrary
            # delay.  Wait until the opened buffer is visibly painted before
            # driving insert/delete/save keystrokes.
            visible = child.expect(["_start:", PROMPT], timeout=remaining())
            if visible == 1:
                raise RuntimeError(
                    "Vim exited before painting the opened buffer; "
                    f"console: {child.before[-500:]}"
                )
            time.sleep(3.0)
            child.send("i")
            time.sleep(0.5)
            for character in "# REM_VIM_REMOVE_ME\r# REM_VIM_EDITED\r":
                child.send(character)
                time.sleep(0.08)
            time.sleep(0.5)
            child.send("\x1b")
            time.sleep(0.5)
            for character in "kdd":
                child.send(character)
                time.sleep(0.2)
            time.sleep(0.4)
            for character in ":wq\r":
                child.send(character)
                time.sleep(0.2)
            child.expect(r"VIM_EDIT_RC:(\d+)", timeout=remaining())
            if child.match.group(1) != "0":
                raise RuntimeError(f"Vim edit/save exited {child.match.group(1)}")
            child.expect(PROMPT, timeout=remaining())

            child.sendline("/bin/cat /root/hello.S; echo FILE_RC:$?")
            child.expect(r"FILE_RC:(\d+)", timeout=remaining())
            file_output = child.before
            if child.match.group(1) != "0" or "REM_VIM_EDITED" not in file_output or "REM_VIM_REMOVE_ME" in file_output:
                raise RuntimeError("saved assembly contents did not match inserted/deleted text")
            child.expect(PROMPT, timeout=remaining())

            child.sendline(
                f"/usr/bin/vim -u NONE -i NONE -n -T {args.term} /root/hello.S; "
                "echo VIM_REOPEN_RC:$?"
            )
            child.expect("REM_VIM_EDITED", timeout=remaining())
            child.send(":q!\r")
            child.expect(r"VIM_REOPEN_RC:(\d+)", timeout=remaining())
            if child.match.group(1) != "0":
                raise RuntimeError(f"Vim reopen exited {child.match.group(1)}")
            child.expect(PROMPT, timeout=remaining())

            for command, marker, expected in (
                ("/root/native/bin/as -o /root/hello.o /root/hello.S", "AS_RC", "0"),
                ("/root/native/bin/ld -T /root/native/lib/ldscripts/elf32myemulator2.x -o /root/hello /root/hello.o", "LD_RC", "0"),
            ):
                child.sendline(f"{command}; echo {marker}:$?")
                child.expect(rf"{marker}:(\d+)", timeout=remaining())
                if child.match.group(1) != expected:
                    raise RuntimeError(f"{marker} returned {child.match.group(1)}")
                child.expect(PROMPT, timeout=remaining())

            child.sendline("/root/hello; echo RUN_RC:$?")
            child.expect("REM NATIVE LINK PASS", timeout=remaining())
            child.expect(r"RUN_RC:(\d+)", timeout=remaining())
            if child.match.group(1) != "0":
                raise RuntimeError(f"edited program exited {child.match.group(1)}")
            child.expect(PROMPT, timeout=remaining())
            text = log_path.read_text(errors="replace")
            if "Segmentation fault" in text or "MYEMU_BAD_USER_FAULT" in text or "EXT4-fs error" in text:
                raise RuntimeError("guest/kernel diagnostics reported a fatal fault or filesystem error")
            status = "PASS"
            detail = "Vim version, interactive edit/delete/save/reopen, native GAS, native ld, and execution passed"
        except pexpect.TIMEOUT:
            status = "TIMEOUT"
            detail = "Vim or the guest did not complete the interactive acceptance sequence"
        except pexpect.EOF:
            status = "CRASH"
            detail = "QEMU or the guest terminated before the acceptance sequence completed"
        except RuntimeError as error:
            status = "FAIL"
            detail = str(error)
        finally:
            stop(child, own_group)

    result = {
        "name": "vim-interactive",
        "path": "/usr/bin/vim",
        "status": status,
        "detail": detail,
        "duration_seconds": time.monotonic() - started,
        "diagnostic_log": str(log_path),
        "artifacts": {
            "qemu": str(args.qemu),
            "kernel": str(args.kernel),
            "rootfs": str(image),
            "qemu_sha256": sha256(args.qemu),
            "kernel_sha256": sha256(args.kernel),
            "rootfs_sha256": sha256(image),
        },
    }
    result_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(result_path)
    print(f"{status}: {detail}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
