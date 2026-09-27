#!/usr/bin/env python3
"""Exercise native GNU Make/GAS/ld and run the resulting REM ELF in a cloned ext4 guest."""

import argparse
import hashlib
import json
import os
import pexpect
import signal
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / ".rem-phone-deploy-20260925/REM-FLIGHT"
SOURCE_ROOTFS = ROOT / ".rem-flight-build-20260924/shutdown-fix/auto-sync-rootfs.ext4"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".rem-native-assembly-test")
    parser.add_argument("--timeout", type=int, default=240)
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    image = out / "native-assembly-rootfs.ext4"
    console_log = out / "console.log"
    qemu_log = out / "qemu.log"
    report_path = out / "result.json"
    if image.exists():
        raise SystemExit(f"refusing to overwrite existing test image: {image}")

    qemu = PACKAGE / "qemu/qemu-system-myemulator32"
    kernel = PACKAGE / "boot/vmlinux"
    for artifact in (qemu, kernel, SOURCE_ROOTFS):
        if not artifact.is_file():
            raise SystemExit(f"missing required artifact: {artifact}")

    # Use a new writable copy; the verified source rootfs is never changed.
    subprocess.run(["cp", "--reflink=auto", "--sparse=always",
                    str(SOURCE_ROOTFS), str(image)], check=True)
    for command in (
        "symlink /usr/bin/myemulator2-elf-as /root/native/bin/as",
        "symlink /usr/bin/myemulator2-elf-ld /root/native/bin/ld",
    ):
        subprocess.run(["debugfs", "-w", "-R", command, str(image)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    qemu_args = ["-M", "myemulator32", "-m", "128M", "-kernel", str(kernel),
                 "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk",
                 "-nographic", "-monitor", "none", "-serial", "stdio",
                 "-icount", "shift=0,sleep=off"]
    deadline = time.monotonic() + args.timeout
    failure = None
    timed_out = False
    command_results = {}
    child = None
    with console_log.open("w", encoding="utf-8", errors="replace") as log, \
            qemu_log.open("w", encoding="utf-8", errors="replace") as qlog:
        try:
            child = pexpect.spawn(str(qemu), qemu_args, encoding="utf-8",
                                  codec_errors="replace", timeout=20,
                                  preexec_fn=os.setsid)
            child.logfile = log
            child.logfile_read = qlog
            child.expect_exact("# ", timeout=min(90, max(1, deadline-time.monotonic())))

            def expect_prompt(prompt="# "):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise pexpect.TIMEOUT("overall guest-test deadline expired")
                child.expect_exact(prompt, timeout=remaining)
                return child.before

            def guest_command(command):
                child.sendline(command)
                return expect_prompt()

            def guest_heredoc(command, lines, terminator):
                child.sendline(command)
                child.expect_exact("> ", timeout=deadline-time.monotonic())
                for line in lines:
                    child.sendline(line)
                    child.expect_exact("> ", timeout=deadline-time.monotonic())
                child.sendline(terminator)
                return expect_prompt()

            command_results["cd"] = guest_command("cd /root")
            guest_heredoc("cat > hello.s <<'REM_ASM'", [
                ".section .text", ".global _start", "_start:",
                "    li r1, 64", "    li r2, 1", "    lui r3, %hi(message)",
                "    ori r3, r3, %lo(message)", "    li r4, message_end-message",
                "    syscall 0", "    li r1, 94", "    li r2, 0", "    syscall 0",
                ".section .rodata", "message:", r'    .ascii "Hello REM!\n"',
                "message_end:"], "REM_ASM")
            guest_heredoc("cat > Makefile <<'REM_MAKE'", [
                ".PHONY: all inspect archive clean", "all: hello", "", "hello: hello.o",
                "\tmyemulator2-elf-ld -T /root/native/lib/ldscripts/elf32myemulator2.x -o $@ $<",
                "\tchmod 755 $@", "", "hello.o: hello.s",
                "\tmyemulator2-elf-as -o $@ $<", "", "inspect: hello",
                "\treadelf -h hello", "\tobjdump -d hello", "", "archive: hello.o",
                "\tar rcs libhello.a hello.o", "\tranlib libhello.a", "\tar t libhello.a",
                "", "clean:", "\trm -f hello hello.o libhello.a"], "REM_MAKE")

            for name, command in (
                ("make", "make --version"),
                ("as", "myemulator2-elf-as --version"),
                ("ld", "myemulator2-elf-ld --version"),
                ("objdump", "objdump --version"),
                ("readelf", "readelf --version"),
                ("ar", "ar --version"),
                ("ranlib", "ranlib --version"),
                ("editor", "test -x /bin/vi; echo VI_PRESENT_RC:$?; which vi"),
            ):
                command_results[name] = guest_command(command)

            command_results["make_build"] = guest_command(
                "make clean && make && make inspect && make archive; echo MAKE_RC:$?")
            command_results["run"] = guest_command("./hello; echo HELLO_RC:$?")
            command_results["sync"] = guest_command("sync")
            command_results["completion"] = guest_command("echo REM_AS_LD_MAKE_ACCEPTANCE_DONE")
        except (pexpect.TIMEOUT, pexpect.EOF) as exc:
            failure = f"{type(exc).__name__}: {exc}"
            timed_out = isinstance(exc, pexpect.TIMEOUT)
        finally:
            if child is not None:
                try:
                    os.killpg(child.pid, signal.SIGTERM)
                    child.wait()
                except (ProcessLookupError, pexpect.ExceptionPexpect):
                    pass
                except Exception:
                    try:
                        os.killpg(child.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    child.close(force=True)

    # Check each command's output independently; echoed input cannot satisfy these.
    checks = {
        "make": "GNU Make" in command_results.get("make", ""),
        "as": "GNU assembler" in command_results.get("as", ""),
        "ld": "GNU ld" in command_results.get("ld", ""),
        "objdump": "GNU objdump" in command_results.get("objdump", ""),
        "readelf": "GNU readelf" in command_results.get("readelf", ""),
        "ar": "GNU ar" in command_results.get("ar", ""),
        "ranlib": "GNU ranlib" in command_results.get("ranlib", ""),
        "editor": "VI_PRESENT_RC:0" in command_results.get("editor", "") and
                  "/bin/vi" in command_results.get("editor", ""),
        "make_build": "MAKE_RC:0" in command_results.get("make_build", ""),
        "run_output": "Hello REM!" in command_results.get("run", ""),
        "run_exit_status": "HELLO_RC:0" in command_results.get("run", ""),
        "completed": "REM_AS_LD_MAKE_ACCEPTANCE_DONE" in
                     command_results.get("completion", ""),
    }
    full_log = console_log.read_text(encoding="utf-8", errors="replace")
    passed = failure is None and all(checks.values()) and "Kernel panic" not in full_log
    result = {
        "status": "PASS" if passed else ("TIMEOUT" if timed_out else "FAIL"),
        "failure": failure,
        "checks": checks,
        "commands": {key: value[-5000:] for key, value in command_results.items()},
        "source_rootfs": str(SOURCE_ROOTFS),
        "source_rootfs_sha256": sha256(SOURCE_ROOTFS),
        "test_rootfs": str(image),
        "test_rootfs_sha256": sha256(image),
        "kernel": str(kernel),
        "kernel_sha256": sha256(kernel),
        "qemu": str(qemu),
        "qemu_sha256": sha256(qemu),
        "qemu_arguments": [str(qemu), *qemu_args],
        "console_log": str(console_log),
        "qemu_log": str(qemu_log),
    }
    report_path.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
