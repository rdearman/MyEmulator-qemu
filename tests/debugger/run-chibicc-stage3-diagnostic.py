#!/usr/bin/env python3
"""Run chibicc Stage-3 probes under REM ptrace in isolated disposable images.

Required artifacts are explicit so this runner cannot silently select a stale
QEMU, kernel, base image, compiler, or debugger. Each case gets a fresh image
copy. QEMU is PID-recorded before exec and terminated/verified on every path.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

import pexpect


REPO = Path(__file__).resolve().parents[2]
TRACKED_QEMU = Path(__file__).with_name("qemu-tracked.sh")
SIGNAL_INPUT = "run\nbt\nregs\ndisasm\ncontinue\nquit\n"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def running_qemus():
    found = []
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        proc = Path("/proc") / entry
        try:
            exe = os.path.basename(os.readlink(proc / "exe"))
            if exe != "qemu-system-myemulator32":
                continue
            args = (proc / "cmdline").read_bytes().replace(b"\0", b" ").decode(
                "utf-8", errors="replace").strip()
            found.append((int(entry), args))
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            continue
    return found


def debugfs(image, commands):
    command_file = image.parent / (image.name + ".debugfs")
    command_file.write_text("\n".join(commands) + "\n", encoding="utf-8")
    try:
        result = subprocess.run(
            ["debugfs", "-w", "-f", str(command_file), str(image)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=120, check=False)
    finally:
        command_file.unlink(missing_ok=True)
    if result.returncode != 0:
        raise RuntimeError(f"debugfs failed ({result.returncode}):\n{result.stdout}")


def guest_init(case):
    header = """#!/bin/busybox ash
exec >/dev/ttyMY0 2>&1
export PATH=/root/native/bin:/usr/bin:/bin:/sbin
cd /root/chibicc-src || exit 90
echo REMDBG_CASE_BEGIN:case_name
run_debugged() {
    printf 'run\\nbt\\nregs\\ndisasm\\ncontinue\\nquit\\n' | /root/remdiag "$@"
    echo REMDBG_COMMAND_RETURN:$?
}
""".replace("case_name", case)
    if case == "hello":
        body = "run_debugged /bin/busybox echo REMDBG_HELLO\n"
    elif case == "small-compile":
        body = """cat > /root/remdiag-tiny.c <<'EOF'
int puts(const char *);
int main(void) { puts("REMDBG_TINY_OK"); return 0; }
EOF
rm -f /root/remdiag-tiny.s /root/remdiag-tiny.o /root/remdiag-tiny
run_debugged /root/remdiag-chibicc -cc1 -DCHIBICC_REM -I/root/chibicc-src \\
  /root/remdiag-tiny.c -cc1-input /root/remdiag-tiny.c -cc1-output /root/remdiag-tiny.s
if test -s /root/remdiag-tiny.s; then
    /root/native/bin/as /root/remdiag-tiny.s -o /root/remdiag-tiny.o
    /root/native/bin/ld -T /root/native/lib/myemulator2-user.ld -o /root/remdiag-tiny \\
      /root/native/lib/crt1.o /root/native/lib/crti.o /root/remdiag-tiny.o \\
      --start-group /root/native/lib/libc.a /root/native/lib/libgcc.a --end-group \\
      /root/native/lib/crtn.o
    run_debugged /root/remdiag-tiny
else
    echo REMDBG_TINY_ASSEMBLY_MISSING
fi
"""
    elif case == "small-compile-stage2":
        body = """cat > /root/remdiag-tiny.c <<'EOF'
int puts(const char *);
int main(void) { puts("REMDBG_TINY_STAGE2_OK"); return 0; }
EOF
rm -f /root/remdiag-tiny.s /root/remdiag-tiny.o /root/remdiag-tiny
printf 'run\\nbt\\nregs\\ndisasm\\ncontinue\\nquit\\n' | /root/remdiag /root/chibicc-stage2 \\
  -cc1 -DCHIBICC_REM -I/root/chibicc-src /root/remdiag-tiny.c -cc1-input /root/remdiag-tiny.c \\
  -cc1-output /root/remdiag-tiny.s
echo REMDBG_COMMAND_RETURN:$?
if test -s /root/remdiag-tiny.s; then
    /root/native/bin/as /root/remdiag-tiny.s -o /root/remdiag-tiny.o
    /root/native/bin/ld -T /root/native/lib/myemulator2-user.ld -o /root/remdiag-tiny \\
      /root/native/lib/crt1.o /root/native/lib/crti.o /root/remdiag-tiny.o \\
      --start-group /root/native/lib/libc.a /root/native/lib/libgcc.a --end-group \\
      /root/native/lib/crtn.o
    run_debugged /root/remdiag-tiny
else
    echo REMDBG_TINY_ASSEMBLY_MISSING:stage2
fi
"""
    elif case in ("main", "parse"):
        body = f"""rm -f {case}.s
run_debugged /root/remdiag-chibicc -cc1 -DCHIBICC_REM -I/root/chibicc-src \\
  {case}.c -cc1-input {case}.c -cc1-output {case}.s
if test -s {case}.s; then echo REMDBG_ASSEMBLY_PRESENT:{case}; else echo REMDBG_ASSEMBLY_MISSING:{case}; fi
"""
    else:
        raise ValueError(case)
    return header + body + f"""
echo REMDBG_CASE_DONE:{case}
sync
/bin/busybox poweroff -f
while :; do /bin/busybox sleep 60; done
"""


def write_case_image(base, image, stage3, remdbg, case):
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", str(base), str(image)],
                   check=True)
    init_host = image.parent / f"init-{case}.sh"
    init_host.write_text(guest_init(case), encoding="utf-8")
    init_host.chmod(0o755)
    debugfs(image, [
        f"write {stage3} /root/remdiag-chibicc",
        "sif /root/remdiag-chibicc mode 0100755",
        f"write {remdbg} /root/remdiag",
        "sif /root/remdiag mode 0100755",
        "rm /sbin/init",
        f"write {init_host} /sbin/init",
        "sif /sbin/init mode 0100755",
    ])
    init_host.unlink(missing_ok=True)


def monitor_quit(guest):
    """Switch this QEMU stdio mux to its HMP monitor and request clean exit."""
    guest.send("\x01c")  # QEMU stdio escape: Ctrl-A, then switch to monitor.
    time.sleep(0.1)
    guest.sendline("quit")


def run_case(args, case, out_dir, tuple_hashes):
    if running_qemus():
        current = "\n".join(f"PID={pid} COMMAND={cmd}" for pid, cmd in running_qemus())
        raise RuntimeError("refusing to start a duplicate QEMU:\n" + current)

    image = out_dir / f"{case}.ext4"
    write_case_image(args.base_rootfs, image, args.stage3, args.remdbg, case)
    image_hash = sha256(image)
    log_path = out_dir / f"{case}.serial.log"
    record_path = out_dir / f"{case}.qemu.txt"
    argv = [
        "-M", "myemulator32", "-m", args.memory, "-kernel", str(args.kernel),
        "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk",
        "-nographic", "-serial", "mon:stdio",
        "-icount", "shift=0,sleep=off", "-no-reboot",
    ]
    purpose = f"REM chibicc Stage-3 ptrace diagnostic: {case}"
    record_path.unlink(missing_ok=True)
    guest = pexpect.spawn(str(TRACKED_QEMU),
                          [str(record_path), purpose, str(args.qemu), *argv],
                          timeout=args.timeout, encoding="utf-8",
                          codec_errors="replace")
    with open(log_path, "w", encoding="utf-8") as log:
        guest.logfile = log
        record_deadline = time.monotonic() + 10
        while not record_path.exists() and time.monotonic() < record_deadline:
            time.sleep(0.02)
        if not record_path.exists():
            guest.close(force=True)
            raise RuntimeError(f"QEMU launch was not recorded: {record_path}")
        record = record_path.read_text(encoding="utf-8")
        if f"PID={guest.pid}\n" not in record or "COMMAND=" not in record:
            guest.close(force=True)
            raise RuntimeError(f"QEMU PID/command record mismatch: {record}")

        status = "TIMEOUT"
        try:
            guest.expect(f"REMDBG_CASE_DONE:{case}", timeout=args.timeout)
            status = "COMPLETED"
            monitor_quit(guest)
            try:
                guest.expect(pexpect.EOF, timeout=10)
            except pexpect.TIMEOUT:
                status = "COMPLETED; QMP QUIT REQUIRED TERMINATION FALLBACK"
        except pexpect.TIMEOUT:
            status = "TIMEOUT"
        finally:
            if guest.isalive():
                guest.kill(signal.SIGTERM)
                try:
                    guest.expect(pexpect.EOF, timeout=5)
                except pexpect.TIMEOUT:
                    guest.kill(signal.SIGKILL)
                    try:
                        guest.expect(pexpect.EOF, timeout=5)
                    except pexpect.TIMEOUT:
                        pass
            guest.close(force=True)

    launched_pid = int(record.split("PID=", 1)[1].splitlines()[0])
    proc_exe = Path(f"/proc/{launched_pid}/exe")
    for _ in range(50):
        if not proc_exe.exists():
            break
        time.sleep(0.1)
    if proc_exe.exists():
        raise RuntimeError(f"tracked QEMU PID {launched_pid} remains after cleanup")
    report = {
        "case": case,
        "status": status,
        "tuple": tuple_hashes,
        "candidate_rootfs_sha256": image_hash,
        "serial_log": str(log_path),
        "qemu_record": str(record_path),
        "qemu_pid": launched_pid,
        "qemu_command": record.split("COMMAND=", 1)[1].strip(),
    }
    output = log_path.read_text(encoding="utf-8", errors="replace")
    if case == "hello":
        report["evidence"] = "PASS" if "REMDBG_HELLO" in output and "program exited with status 0" in output else "FAIL"
    elif case.startswith("small-compile"):
        marker = "REMDBG_TINY_STAGE2_OK" if case.endswith("stage2") else "REMDBG_TINY_OK"
        report["evidence"] = "PASS" if marker in output and "program exited with status 0" in output else "FAIL"
    else:
        signal_match = any(
            f"stopped: signal {sig} (" in output
            for sig in (1, 4, 7, 8, 11, 12, 13, 15))
        report["evidence"] = "SIGNAL CAPTURED" if signal_match else "NO PTRACE SIGNAL STOP"
        report["signal_capture"] = signal_match
        report["fault_address_captured"] = "fault address=0x" in output
        report["registers_captured"] = "pc=" in output and "sp=r13=" in output
        report["backtrace_captured"] = "#0 0x" in output
        report["assembly_present"] = f"REMDBG_ASSEMBLY_PRESENT:{case}" in output
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", type=Path, required=True)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--base-rootfs", type=Path, required=True,
                        help="disposable Stage-3 test image containing /root/chibicc-src")
    parser.add_argument("--stage3", type=Path, required=True,
                        help="exact Stage-3 REM ELF under investigation")
    parser.add_argument("--remdbg", type=Path, required=True,
                        help="REM ELF rebuilt from the local remdbg source")
    parser.add_argument("--out-dir", type=Path,
                        default=Path("/tmp/remdbg-chibicc-stage3-diagnostic"))
    parser.add_argument("--memory", default="512M")
    parser.add_argument("--timeout", type=int, default=480,
                        help="per-case QEMU timeout in seconds")
    args = parser.parse_args()

    paths = [args.qemu, args.kernel, args.base_rootfs, args.stage3, args.remdbg]
    for path in paths:
        if not path.is_file():
            parser.error(f"required artifact is missing: {path}")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    tuple_hashes = {
        "qemu_sha256": sha256(args.qemu),
        "kernel_sha256": sha256(args.kernel),
        "base_rootfs_sha256": sha256(args.base_rootfs),
        "stage3_sha256": sha256(args.stage3),
        "remdbg_sha256": sha256(args.remdbg),
    }
    (args.out_dir / "artifact-tuple.json").write_text(
        json.dumps(tuple_hashes, indent=2) + "\n", encoding="utf-8")

    results = []
    for case in ("hello", "small-compile-stage2", "small-compile", "main", "parse"):
        print(f"RUN {case}", flush=True)
        result = run_case(args, case, args.out_dir, tuple_hashes)
        results.append(result)
        print(json.dumps(result, indent=2), flush=True)
    report_path = args.out_dir / "report.json"
    report_path.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(f"REPORT {report_path}")
    return 0 if results[0]["evidence"] == "PASS" and results[1]["evidence"] == "PASS" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("interrupted; tracked QEMU cleanup runs in the active test case", file=sys.stderr)
        raise
