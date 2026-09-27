#!/usr/bin/env python3
"""Run the REM userspace regression script in one disposable QEMU guest."""

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

ROOT = Path(__file__).resolve().parents[2]
GUEST = ROOT / "toolchain/userspace/guest-regression.sh"
MARKER = re.compile(r"^echo 'REMTEST_BEGIN\|([^|]+)\|([^']+)'$")
END_MARKER = re.compile(r'^.*REMTEST_END\|([^|]+)\|\$\?"$')


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def guest_fault_pc(log_path):
    if not log_path.is_file():
        return None
    match = re.search(r"\bpc=(?:0x)?([0-9a-fA-F]{6,})",
                      log_path.read_text(errors="replace"))
    return f"0x{match.group(1)}" if match else None


def inject(image, host_script):
    command = ["debugfs", "-w", "-R", f"write {host_script} /tmp/rem-userspace-regression.sh", str(image)]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def sparse_copy(source, destination):
    """Copy a disk image without materializing its unallocated ext4 space."""
    subprocess.run(["cp", "--sparse=always", "--reflink=auto",
                    str(source), str(destination)], check=True)


def test_blocks():
    lines = GUEST.read_text().splitlines()
    blocks = []
    for index, line in enumerate(lines):
        match = MARKER.match(line)
        if not match or index + 1 >= len(lines):
            continue
        end = END_MARKER.match(lines[index + 1])
        if not end or end.group(1) != match.group(1):
            raise RuntimeError(f"malformed guest test block near line {index + 1}")
        blocks.append((match.group(1), match.group(2), lines[index:index + 2]))
    if not blocks:
        raise RuntimeError(f"no REMTEST blocks found in {GUEST}")
    return blocks


def make_guest_script(block, destination, show_output=False, dump_faults=False):
    command = block[1].split('; echo "REMTEST_END', 1)[0]
    if show_output:
        command = re.sub(r"\s+>\s+/tmp/[^\s]+\s+2>&1\s*$", "", command)
    if not command.lstrip().startswith('/'):
        raise RuntimeError(f"cannot convert guest test command to argv: {command}")
    marker = MARKER.match(block[0])
    if not marker:
        raise RuntimeError(f"malformed test marker: {block[0]}")
    name, path = marker.groups()
    dump = ("if [ \"$rc\" -ge 128 ]; then "
            "/bin/busybox dmesg | /bin/busybox grep MYEMU_BAD_USER_FAULT; fi\n"
            if dump_faults else "")
    destination.write_text(
        "#!/bin/sh\n"
        + f"echo 'REMTEST_BEGIN|{name}|{path}'\n"
        + f"{command}; rc=$?\n"
        + dump
        + f"echo \"REMTEST_END|{name}|$rc\"\n"
    )
    destination.chmod(0o755)


def seed_test_files(image):
    """Install deterministic disposable inputs without running guest code."""
    seeds = {
        "/tmp/rem-userspace-input.txt": "REM_TEST_LINE_1\nREM_TEST_LINE_2\n",
        "/tmp/rem-userspace-touched": "",
        "/tmp/rem-userspace-copied": "",
        "/tmp/rem-userspace-moved": "",
    }
    for guest, contents in seeds.items():
        host = Path(tempfile.mkstemp(prefix="rem-seed-")[1])
        try:
            host.write_text(contents)
            subprocess.run(["debugfs", "-w", "-R", f"rm {guest}", str(image)],
                           check=False, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            subprocess.run(["debugfs", "-w", "-R", f"write {host} {guest}", str(image)],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        finally:
            host.unlink(missing_ok=True)


def classify(rc):
    if rc == 0:
        return "PASS"
    if rc == 127:
        return "SKIP"
    if rc >= 128:
        return "CRASH"
    return "FAIL"


def collect_results(log_path):
    """Parse completed and in-flight guest tests from a serial log."""
    tests = []
    open_tests = {}
    if not log_path.exists():
        return tests, open_tests
    text = log_path.read_text(errors="replace")
    for line in text.splitlines():
        if line.startswith("REMTEST_BEGIN|"):
            _, name, path = line.split("|", 2)
            open_tests[name] = {"name": name, "path": path,
                                "started": time.monotonic()}
        elif line.startswith("REMTEST_END|"):
            _, name, raw_rc = line.split("|", 2)
            item = open_tests.pop(name, {"name": name, "path": ""})
            rc = int(raw_rc)
            item.update({"status": classify(rc), "exit_status": rc,
                         "duration_seconds": 0})
            tests.append(item)
    return tests, open_tests


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rootfs", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("/tmp/rem-userspace-regression"))
    parser.add_argument("--qemu", type=Path, default=ROOT / ".qemu-build/qemu-system-myemulator32")
    parser.add_argument("--kernel", type=Path, default=ROOT / ".linux-build/build/vmlinux")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--memory", default="16M")
    parser.add_argument("--test")
    parser.add_argument("--group")
    parser.add_argument("--guest-shell", choices=("bash", "ash"), default="bash",
                        help="shell used only to launch the isolated guest test")
    parser.add_argument("--qemu-trace", action="store_true")
    parser.add_argument("--qemu-log-items",
                        help="QEMU -d log items, e.g. int,guest_errors (less verbose than --qemu-trace)")
    parser.add_argument("--qemu-log-filter",
                        help="QEMU -dfilter address range(s), e.g. 0x02007a98..0x02007d1c")
    parser.add_argument("--show-output", action="store_true",
                        help="leave the tested command's stdout/stderr on the serial console")
    parser.add_argument("--dump-faults", action="store_true",
                        help="print MYEMU_BAD_USER_FAULT records from dmesg after a crashing test")
    parser.add_argument("--no-icount", action="store_true",
                        help="diagnostic mode: use normal QEMU timing")
    args = parser.parse_args()
    if not args.rootfs.is_file():
        parser.error(f"missing rootfs: {args.rootfs}")
    if not args.qemu.is_file() or not args.kernel.is_file():
        parser.error("QEMU or kernel artifact is missing")
    args.output.mkdir(parents=True, exist_ok=True)
    blocks = test_blocks()
    if args.test:
        blocks = [item for item in blocks if item[0] == args.test]
    if args.group:
        blocks = [item for item in blocks if item[0].split('-', 1)[0] == args.group]
    if not blocks:
        parser.error("requested test/group does not exist")
    tested_guest_binaries = {}
    if any(path == "/usr/bin/bash" for _, path, _ in blocks):
        with tempfile.TemporaryDirectory(prefix="rem-userspace-binary-") as tmpdir:
            host_binary = Path(tmpdir) / "bash"
            subprocess.run(["debugfs", "-R", f"dump /usr/bin/bash {host_binary}",
                            str(args.rootfs)], check=True, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE)
            comment = subprocess.run(["readelf", "-p", ".comment", str(host_binary)],
                                     check=False, text=True, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT).stdout
            compiler_match = re.search(r"GCC: \(GNU\) ([^\n]+)", comment)
            tested_guest_binaries["/usr/bin/bash"] = {
                "sha256": sha256(host_binary),
                "compiler_used": f"GCC {compiler_match.group(1).strip()}" if compiler_match else "UNKNOWN",
                "elf_comment": comment.strip(),
            }
    report = {"artifacts": {"qemu": str(args.qemu), "kernel": str(args.kernel), "rootfs": str(args.rootfs),
                             "qemu_sha256": sha256(args.qemu), "kernel_sha256": sha256(args.kernel),
                             "rootfs_sha256": sha256(args.rootfs),
                             "guest_shell": args.guest_shell},
              "tested_guest_binaries": tested_guest_binaries,
              "tests": [], "status": "COMPLETED"}
    started = time.monotonic()
    for name, path, block in blocks:
        test_dir = args.output / "tests" / name
        test_dir.mkdir(parents=True, exist_ok=True)
        image = test_dir / "rootfs.ext4"
        script = test_dir / "test.sh"
        log_path = test_dir / "console.log"
        sparse_copy(args.rootfs, image)
        seed_test_files(image)
        make_guest_script(block, script, args.show_output, args.dump_faults)
        inject(image, script)
        qemu_args = [str(args.qemu), "-M", "myemulator32", "-m", args.memory, "-kernel", str(args.kernel),
                     "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk", "-nographic",
                     "-monitor", "none"]
        if not args.no_icount:
            qemu_args += ["-icount", "shift=0,sleep=off"]
        if args.qemu_log_items:
            qemu_args += ["-d", args.qemu_log_items, "-D", str(test_dir / "qemu-trace.log")]
            if args.qemu_log_filter:
                qemu_args += ["-dfilter", args.qemu_log_filter]
        elif args.qemu_trace:
            qemu_args += ["-d", "in_asm,exec,cpu,guest_errors,int", "-D", str(test_dir / "qemu-trace.log")]
                # Give every guest its own process group where the host permits it.
        # Some constrained runners reject a pty pre-exec setsid, so establish
        # the group from the parent and retain a direct-child fallback.
        child = pexpect.spawn(qemu_args[0], qemu_args[1:], timeout=1,
                              encoding="utf-8", codec_errors="replace")
        owns_process_group = False
        try:
            os.setpgid(child.pid, child.pid)
            owns_process_group = True
        except (PermissionError, ProcessLookupError):
            pass
        test_started = time.monotonic()
        status = "NOT TESTED"
        detail = ""
        raw_rc = None
        try:
            with log_path.open("w") as log:
                child.logfile = log
                child.expect(r"# ", timeout=args.timeout)
                # Selectable because either shell may itself be under test;
                # each VM runs exactly one application test.
                launcher = ("/usr/bin/bash" if args.guest_shell == "bash"
                            else "/bin/busybox sh")
                child.sendline(f'{launcher} /tmp/rem-userspace-regression.sh')
                outcome = child.expect(
                    [rf"REMTEST_END\|{re.escape(name)}\|[0-9]+", r"\n# "],
                    timeout=args.timeout)
                if outcome == 1:
                    # The prompt returned without a test-end marker: the
                    # guest launcher died before reporting this application.
                    log_text = log_path.read_text(errors="replace")
                    if ("MYEMU_BAD_USER_FAULT" in log_text or
                            "Segmentation fault" in log_text or
                            "Bus error" in log_text or
                            "Kernel panic" in log_text):
                        status = "CRASH"
                        detail = "guest fault occurred before the test completion marker"
                    else:
                        status = "NOT TESTED"
                        detail = f"{args.guest_shell} launcher exited before the test ran"
                else:
                    completed, _ = collect_results(log_path)
                    if completed:
                        item = completed[-1]
                        status = item["status"]
                        raw_rc = item.get("exit_status")
                    else:
                        status = "CRASH"
        except pexpect.TIMEOUT:
            status = "TIMEOUT"
            detail = "guest did not report completion before the per-test timeout"
        except pexpect.EOF:
            status = "CRASH"
            detail = "QEMU or guest terminated before the test completion marker"
        finally:
            try:
                # The test owns this process group.  Always reap it, even
                # after the guest emitted its result marker; QEMU may still
                # be running PID 1 while the shell has returned.
                try:
                    (os.killpg if owns_process_group else os.kill)(child.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                deadline = time.monotonic() + 0.25
                while child.isalive() and time.monotonic() < deadline:
                    time.sleep(0.02)
                if child.isalive():
                    try:
                        (os.killpg if owns_process_group else os.kill)(child.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            except ProcessLookupError:
                pass
            # close(force=True) reaps the child after the group has been
            # terminated.  Keep cleanup bounded so a broken guest cannot
            # block later independent tests.
            try:
                child.close(force=True)
            except (OSError, pexpect.ExceptionPexpect):
                pass
        command = block[1].split('; echo "REMTEST_END', 1)[0]
        report_item = {"name": name, "path": path, "status": status,
                                "detail": detail,
                                "exit_status": raw_rc, "duration_seconds": time.monotonic() - test_started,
                                "command": command,
                                "minimal_reproducer": command,
                                "fault_pc": guest_fault_pc(log_path),
                                "failure_layer": ("userspace SIGSEGV; causal subsystem unresolved"
                                                  if status == "CRASH" else
                                                  "nonzero guest result; causal subsystem unresolved"
                                                  if status == "FAIL" else None),
                                "diagnostic_log": str(log_path)}
        if path in tested_guest_binaries:
            report_item["binary_sha256"] = tested_guest_binaries[path]["sha256"]
            report_item["compiler_used"] = tested_guest_binaries[path]["compiler_used"]
        report["tests"].append(report_item)
        detail = ""
    report["duration_seconds"] = time.monotonic() - started
    report["summary"] = {status: sum(item.get("status") == status for item in report["tests"])
                         for status in ("PASS", "FAIL", "CRASH", "TIMEOUT", "SKIP", "NOT TESTED")}
    if any(report["summary"][status] for status in
           ("FAIL", "CRASH", "TIMEOUT", "NOT TESTED")):
        report["status"] = "COMPLETED_WITH_FAILURES"
    (args.output / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    (args.output / "summary.txt").write_text("\n".join([f"overall: {report['status']}"] +
        [f"{item['status']:7} {item['name']} {item['path']} rc={item.get('exit_status', '?')}" for item in report["tests"]]) + "\n")
    print(args.output / "report.json")
    return 0 if report["status"] == "COMPLETED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
