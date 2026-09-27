#!/usr/bin/env python3
"""Exercise REM ext4 persistence through repeated real guest boots.

This host-side test takes a disposable image copy. It uses the serial shell,
calls sync, exits the supervised shell (whose PID 1 syncs again), and only then
stops QEMU. The current guest has no working poweroff/remount-read-only syscall,
so this is a synchronized stop followed by normal ext4 journal recovery at the
next boot, not a claim of a clean kernel unmount.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import signal
import subprocess
import time
from pathlib import Path

import pexpect

PROMPT = r"\n(?:# |REM# ?|/ # ?)"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def quote(value: str) -> str:
    return "'" + value.replace("'", "'\\''") + "'"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=Path, required=True)
    parser.add_argument("--qemu", type=Path, required=True)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cycles", type=int, default=3)
    parser.add_argument("--memory", default="128M")
    parser.add_argument("--timeout", type=int, default=150)
    parser.add_argument("--small-only", action="store_true",
                        help="skip multi-block size fixtures after a guest large-write failure")
    parser.add_argument("--tiny-only", action="store_true",
                        help="test only one small file across reboot cycles")
    parser.add_argument("--icount", default="shift=0,sleep=off",
                        help="QEMU instruction-count setting (default: flight package setting); use 'off' to disable")
    args = parser.parse_args()
    for path in (args.seed, args.qemu, args.kernel):
        if not path.is_file():
            parser.error(f"missing file: {path}")
    if args.cycles < 2:
        parser.error("--cycles must be at least 2")
    args.output.mkdir(parents=True, exist_ok=False)
    image = args.output / "rem-persistence.ext4"
    subprocess.run(["cp", "--sparse=auto", "--reflink=auto", str(args.seed), str(image)], check=True)
    seed_hash = file_sha256(args.seed)
    report = {
        "status": "RUNNING",
        "seed": str(args.seed.resolve()),
        "seed_sha256": seed_hash,
        "test_image": str(image.resolve()),
        "qemu": str(args.qemu.resolve()),
        "qemu_sha256": file_sha256(args.qemu),
        "kernel": str(args.kernel.resolve()),
        "kernel_sha256": hashlib.sha256(args.kernel.read_bytes()).hexdigest(),
        "memory": args.memory,
        "qemu_icount": args.icount,
        "small_only": args.small_only,
        "tiny_only": args.tiny_only,
        "cycles": [],
    }
    transcript = args.output / "serial.log"
    expected: dict[str, tuple[bytes, int]] = {}

    def q(command: str) -> str:
        return quote(command)

    def boot(cycle: int):
        argv = [str(args.qemu.resolve()), "-M", "myemulator32", "-m", args.memory,
                "-kernel", str(args.kernel.resolve()), "-drive",
                f"file={image.resolve()},format=raw,if=none,id=myemulator2-disk",
                "-nographic", "-monitor", "none", "-serial", "stdio"]
        if args.icount != "off":
            argv.extend(["-icount", args.icount])
        child = pexpect.spawn(argv[0], argv[1:], timeout=args.timeout,
                              encoding="utf-8", codec_errors="replace")
        child.delaybeforesend = 0
        log = transcript.open("a", encoding="utf-8")
        child.logfile = log
        child.expect(PROMPT, timeout=args.timeout)
        report["cycles"].append({"cycle": cycle, "boot": "shell prompt reached", "commands": []})
        return child, log

    def command(child, log, line: str, name: str, expected_text: str | None = None):
        marker = f"C7_{name}_{len(report['cycles'][-1]['commands'])}"
        child.sendline(f"{line}; echo {marker}:$?")
        matched = child.expect([r"\n" + marker + r":(\d+)",
                                r"Kernel panic|MYEMU_FATAL_REGS|MyEmulator2 exception",
                                PROMPT],
                               timeout=args.timeout)
        if matched == 1:
            raise RuntimeError(f"guest kernel exception while running {line}")
        if matched == 2:
            raise RuntimeError(f"guest shell rejected command before status marker: {line}")
        output = child.before
        status = child.match.group(1)
        if status != "0":
            raise RuntimeError(f"guest command returned {status}: {line}\n{output[-1000:]}")
        if expected_text is not None and expected_text not in output:
            raise RuntimeError(f"guest output missing {expected_text!r}: {line}\n{output[-1000:]}")
        child.expect(PROMPT, timeout=args.timeout)
        report["cycles"][-1]["commands"].append({"name": name, "status": int(status)})
        log.flush()
        return output

    def stop_synchronized(child, log):
        command(child, log, "/bin/busybox sync", "sync")
        child.sendline("exit")
        child.expect("REM shell exited", timeout=args.timeout)
        child.expect(PROMPT, timeout=args.timeout)
        report["cycles"][-1]["shutdown"] = "sync + supervised shell exit (PID 1 sync); QEMU stopped afterward"
        os.kill(child.pid, signal.SIGTERM)
        child.wait()
        log.close()

    def repeated_write(child, log, path: str, size: int, label: str) -> bytes:
        payload = "0123456789abcdef"
        repetitions = size // len(payload)
        command(child, log, f": > {q(path)}", label + "-truncate")
        batch_size = 32
        for batch, start in enumerate(range(0, repetitions, batch_size)):
            stop = min(start + batch_size, repetitions)
            for _ in range(start, stop):
                child.sendline(f"printf '%s' '{payload}' >> {q(path)}")
            marker = f"C7_{label}_{batch}"
            child.sendline(f"echo {marker}:$?")
            matched = child.expect([r"\n" + marker + r":(\d+)",
                                    r"Kernel panic|MYEMU_FATAL_REGS|MyEmulator2 exception",
                                    PROMPT], timeout=args.timeout)
            if matched == 1:
                raise RuntimeError(f"guest kernel exception during repeated write to {path}")
            if matched == 2:
                raise RuntimeError(f"guest shell rejected repeated write to {path}")
            if child.match.group(1) != "0":
                raise RuntimeError(f"guest writes failed for {path}: {child.match.group(1)}")
            child.expect(PROMPT, timeout=args.timeout)
            log.flush()
        return payload.encode() * repetitions

    try:
        for cycle in range(1, args.cycles + 1):
            child, log = boot(cycle)
            try:
                # Verify what the previous cleanly synchronized shell cycle
                # left behind before making any new edits.
                if args.tiny_only:
                    for index, (path, (content, _mode)) in enumerate(expected.items()):
                        command(child, log, f"cat {q(path)}", f"reboot-content-{index}", content.decode())
                else:
                    for index, (path, (content, mode)) in enumerate(expected.items()):
                        output = command(child, log, f"sha256sum {q(path)}", f"reboot-hash-{index}")
                        if sha256(content) not in output:
                            raise RuntimeError(f"reboot hash mismatch for {path}; expected {sha256(content)}")
                        command(child, log, f"test \"$(wc -c < {q(path)})\" -eq {len(content)}", f"reboot-size-{index}")
                        if mode & 0o111:
                            command(child, log, f"test -x {q(path)}", f"reboot-mode-x-{index}")
                        else:
                            command(child, log, f"test ! -x {q(path)}", f"reboot-mode-no-x-{index}")
                if args.tiny_only:
                    path = "/root/codex7-persistence.txt"
                    text = f"REM C7 persistence cycle {cycle}"
                    content = (text + "\n").encode()
                    if cycle == 1:
                        command(child, log, "mkdir -p /root", "mkdir-root")
                    command(child, log, f"printf '%s\\n' {q(text)} > {q(path)}", f"write-tiny-{cycle}")
                    expected[path] = (content, 0o644)
                    command(child, log, f"cat {q(path)}", f"read-tiny-{cycle}", content.decode())
                    stop_synchronized(child, log)
                    continue
                if cycle == 1:
                    command(child, log, "mkdir -p /root/codex7-project/src /root/codex7-project/build /root/codex7-project/logs", "mkdir")
                    command(child, log, "printf 'REM tiny persistence file\\n' > /root/codex7-project/tiny.txt", "tiny")
                    expected["/root/codex7-project/tiny.txt"] = (b"REM tiny persistence file\n", 0o644)
                    if not args.small_only:
                        for name, size in (("several-kb.bin", 8192), ("larger.bin", 65536)):
                            path = f"/root/codex7-project/{name}"
                            # Avoid both the guest's broken large-write path and
                            # ash arithmetic; issue many short writes in batches.
                            content = repeated_write(child, log, path, size, name.replace(".", "-"))
                            expected[path] = (content, 0o644)
                    command(child, log, "printf 'rename me\\n' > /root/codex7-project/rename.tmp && mv /root/codex7-project/rename.tmp /root/codex7-project/renamed.txt", "rename")
                    expected["/root/codex7-project/renamed.txt"] = (b"rename me\n", 0o644)
                    command(child, log, "printf 'delete me\\n' > /root/codex7-project/delete.tmp && rm /root/codex7-project/delete.tmp", "delete")
                    command(child, log, "printf 'old\\n' > /root/codex7-project/modify.txt && printf 'new content\\n' > /root/codex7-project/modify.txt", "modify")
                    expected["/root/codex7-project/modify.txt"] = (b"new content\n", 0o644)
                # Development-style source tree. Cycle 2 and later replace/edit
                # files, leaving the previous result across a real reboot.
                version = f"cycle {cycle}"
                sources = {
                    "/root/codex7-project/src/main.c": f'#include "api.h"\nint main(void) {{ return answer(); }} /* {version} */\n',
                    "/root/codex7-project/src/api.h": f"int answer(void); /* {version} */\n",
                    "/root/codex7-project/src/start.s": f".text\n.globl _start\n_start: /* {version} */\n",
                    "/root/codex7-project/build/main.o": "\177ELF\001\001\001REM object fixture " + version + "\n",
                    "/root/codex7-project/build/demo": "\177ELF\001\001\001REM executable fixture " + version + "\n",
                    "/root/codex7-project/logs/build.log": f"build output {version}\n",
                }
                for path, content in sources.items():
                    parent = str(Path(path).parent)
                    command(child, log, f"mkdir -p {q(parent)} && printf '%s' {q(content)} > {q(path)}", "write-" + Path(path).name)
                    mode = 0o755 if path.endswith("/demo") else 0o644
                    if mode == 0o755:
                        command(child, log, f"chmod 755 {q(path)}", "chmod-demo")
                    expected[path] = (content.encode(), mode)
                command(child, log, "printf 'append-like replacement\\n' > /root/codex7-project/logs/latest.log", "log-replace")
                expected["/root/codex7-project/logs/latest.log"] = (b"append-like replacement\n", 0o644)
                # Check every extant workload file by hash, byte length, and
                # executable bit. A missing path or bad command is a hard fail.
                for index, (path, (content, mode)) in enumerate(expected.items()):
                    output = command(child, log, f"sha256sum {q(path)}", f"verify-{index}")
                    if sha256(content) not in output:
                        raise RuntimeError(f"guest hash mismatch for {path}; expected {sha256(content)}")
                    command(child, log, f"test \"$(wc -c < {q(path)})\" -eq {len(content)}", f"size-{index}")
                    if mode & 0o111:
                        command(child, log, f"test -x {q(path)}", f"mode-x-{index}")
                    else:
                        command(child, log, f"test ! -x {q(path)}", f"mode-no-x-{index}")
                command(child, log, "test ! -e /root/codex7-project/delete.tmp && test ! -e /root/codex7-project/rename.tmp", "directory-state")
                stop_synchronized(child, log)
            except BaseException:
                if child.isalive():
                    os.kill(child.pid, signal.SIGTERM)
                    child.wait()
                log.close()
                raise
        report["status"] = "PASS"
    except (pexpect.TIMEOUT, pexpect.EOF, RuntimeError, OSError) as exc:
        report["status"] = "FAIL"
        report["error"] = str(exc)
    finally:
        report["test_image_sha256"] = file_sha256(image)
        (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
