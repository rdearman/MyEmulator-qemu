#!/usr/bin/env python3
"""Verify Linux installs an aligned physical double-fault stack at boot."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

import pexpect


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def qemu_processes():
    found = []
    for proc in Path("/proc").glob("[0-9]*"):
        try:
            if (proc / "exe").resolve().name == "qemu-system-myemulator32":
                found.append((int(proc.name), (proc / "cmdline").read_bytes()
                              .replace(b"\0", b" ").decode(errors="replace")))
        except (OSError, RuntimeError):
            continue
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qemu", type=Path, required=True)
    ap.add_argument("--kernel", type=Path, required=True)
    ap.add_argument("--rootfs", type=Path, required=True)
    ap.add_argument("--log-dir", type=Path, required=True)
    ap.add_argument("--timeout", type=int, default=90)
    ap.add_argument("--wait-init", action="store_true",
                    help="wait until Linux starts /sbin/init before checking DFSP")
    args = ap.parse_args()
    for path in (args.qemu, args.kernel, args.rootfs):
        if not path.is_file():
            ap.error(f"missing required file: {path}")
    args.log_dir.mkdir(parents=True, exist_ok=True)
    serial = args.log_dir / "serial.log"
    record = args.log_dir / "qemu.record"
    result_file = args.log_dir / "result.json"
    serial.unlink(missing_ok=True)
    qemu_cmd = [str(args.qemu), "-M", "myemulator32", "-m", "512M",
                "-kernel", str(args.kernel), "-append",
                "console=ttyMY0,115200 earlycon=myemulator2,0xf0000000 "
                "root=/dev/myemu0 rootflags=noatime rw init=/sbin/init "
                "virtio_mmio.device=0x200@0xf0200000:5 ip=off "
                "myemu.tty_rows=24 myemu.tty_cols=80", "-snapshot",
                "-drive", f"file={args.rootfs},format=raw,if=none,id=myemulator2-disk",
                "-display", "none", "-serial", f"file:{serial}",
                "-monitor", "stdio", "-no-reboot"]
    existing = qemu_processes()
    if any(str(args.rootfs) in cmd for _, cmd in existing):
        raise SystemExit("refusing to start duplicate QEMU on this rootfs")
    record.write_text(f"PURPOSE=verify Linux DFSP initialization\nCOMMAND={qemu_cmd!r}\n")
    proc = None
    data = {"purpose": "verify Linux DFSP initialization",
            "qemu": str(args.qemu), "qemu_sha256": sha256(args.qemu),
            "kernel": str(args.kernel), "kernel_sha256": sha256(args.kernel),
            "rootfs": str(args.rootfs), "rootfs_sha256": sha256(args.rootfs),
            "command": qemu_cmd, "preexisting_qemu": existing}
    nm = subprocess.run(["nm", "-n", str(args.kernel)], check=True,
                        text=True, capture_output=True).stdout
    symbol = re.search(r"^([0-9a-fA-F]+) [A-Za-z] __myemulator2_df_stack_top$",
                       nm, re.M)
    data["expected_dfsp_symbol"] = (f"0x{int(symbol.group(1), 16):08x}"
                                    if symbol else None)
    try:
        proc = pexpect.spawn(str(args.qemu), qemu_cmd[1:], encoding="utf-8",
                             timeout=3, env={**os.environ, "TERM": "linux",
                                            "TMPDIR": str(args.log_dir)})
        data["pid"] = proc.pid
        with record.open("a") as f:
            f.write(f"PID={proc.pid}\n")
        proc.expect(r"\(qemu\)\s*")
        deadline = time.monotonic() + args.timeout
        boot_marker = ("Run /sbin/init as init process" if args.wait_init
                       else "MyEmulator2 RAM:")
        while time.monotonic() < deadline:
            if serial.exists() and boot_marker in serial.read_text(errors="replace"):
                break
            if not proc.isalive():
                raise RuntimeError("QEMU exited before Linux reached early RAM setup")
            time.sleep(1)
        else:
            raise TimeoutError(f"Linux did not reach boot marker: {boot_marker}")
        regs = ""
        while time.monotonic() < deadline:
            if not proc.isalive():
                raise RuntimeError("QEMU exited before DFSP could be queried")
            proc.sendline("info registers")
            proc.expect(r"\(qemu\)\s*")
            regs = proc.before
            m = re.search(r"DFSP:\s*0x([0-9a-fA-F]+)", regs)
            if m:
                value = int(m.group(1), 16)
                data["dfsp"] = f"0x{value:08x}"
                data["registers"] = regs[-3000:]
                if value == 0 or (value & 3):
                    raise RuntimeError(f"DFSP is not nonzero and 4-byte aligned: {data['dfsp']}")
                if symbol and value != int(symbol.group(1), 16):
                    raise RuntimeError(f"DFSP does not match stack top symbol: {data['dfsp']} != {data['expected_dfsp_symbol']}")
                break
            time.sleep(1)
        else:
            raise TimeoutError("timed out waiting for QEMU DFSP register output")
        proc.sendline("quit")
        proc.expect(pexpect.EOF, timeout=10)
        proc.close()
        if proc.exitstatus not in (0, None):
            raise RuntimeError(f"QEMU exited with status {proc.exitstatus}")
        remaining = qemu_processes()
        data["remaining_qemu"] = remaining
        if remaining:
            raise RuntimeError(f"QEMU remains after shutdown: {remaining}")
        data["status"] = "PASS"
    except Exception as e:
        data["status"] = "FAIL"
        data["error"] = repr(e)
        if proc is not None and proc.isalive():
            try:
                proc.sendline("quit")
                proc.expect(pexpect.EOF, timeout=5)
            except Exception:
                try:
                    os.kill(proc.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    proc.expect(pexpect.EOF, timeout=5)
                except Exception:
                    try:
                        os.kill(proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            proc.close(force=True)
        data["remaining_qemu"] = qemu_processes()
    data["serial_sha256"] = sha256(serial) if serial.exists() else None
    data["serial_bytes"] = serial.stat().st_size if serial.exists() else 0
    tmp = result_file.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(data, indent=2) + "\n")
    os.replace(tmp, result_file)
    print(json.dumps(data, indent=2))
    return 0 if data["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
