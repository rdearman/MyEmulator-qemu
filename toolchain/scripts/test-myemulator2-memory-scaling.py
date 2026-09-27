#!/usr/bin/env python3
"""Boot disposable MyEmulator2 VMs at multiple RAM sizes and verify discovery."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import time


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def qmp_quit(path: Path) -> None:
    deadline = time.monotonic() + 10
    last_error: OSError | None = None
    while time.monotonic() < deadline:
        try:
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(2)
                client.connect(str(path))
                file = client.makefile("rwb", buffering=0)
                file.readline()  # QMP greeting
                file.write(b'{"execute":"qmp_capabilities"}\r\n')
                file.readline()
                file.write(b'{"execute":"quit"}\r\n')
                return
        except OSError as error:
            last_error = error
            time.sleep(0.05)
    raise RuntimeError(f"QMP unavailable at {path}: {last_error}")


def parse_size(value: str) -> tuple[str, int]:
    match = re.fullmatch(r"([1-9][0-9]*)([MG])", value.upper())
    if not match:
        raise argparse.ArgumentTypeError("RAM size must be e.g. 32M, 128M, 2G")
    amount = int(match.group(1))
    multiplier = 1024 * 1024 if match.group(2) == "M" else 1024**3
    return value.upper(), amount * multiplier


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", type=Path, required=True)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--rootfs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--memory", nargs="+", default=["32M", "128M", "2G"])
    args = parser.parse_args()

    qemu, kernel, rootfs, output = (p.resolve() for p in
                                    (args.qemu, args.kernel, args.rootfs, args.output))
    output.mkdir(parents=True, exist_ok=True)
    temp_dir = output / "tmp"
    temp_dir.mkdir(exist_ok=True)
    env = os.environ.copy()
    env["TMPDIR"] = str(temp_dir)
    report: dict[str, object] = {
        "qemu": str(qemu), "qemu_sha256": sha256(qemu),
        "kernel": str(kernel), "kernel_sha256": sha256(kernel),
        "rootfs": str(rootfs), "rootfs_sha256": sha256(rootfs),
        "results": [],
    }
    overall_ok = True

    for raw_size in args.memory:
        label, byte_count = parse_size(raw_size)
        slug = label.lower()
        serial_log = output / f"boot-{slug}.serial.log"
        qmp_socket = output / f"qmp-{slug}.sock"
        qmp_socket.unlink(missing_ok=True)
        command = [
            str(qemu), "-M", "myemulator32", "-m", label,
            "-kernel", str(kernel), "-drive",
            f"file={rootfs},format=raw,if=none,id=myemulator2-disk",
            "-display", "none", "-monitor", "none", "-serial", "stdio",
            "-qmp", f"unix:{qmp_socket},server=on,wait=off",
            "-icount", "shift=0,sleep=off", "-snapshot",
        ]
        started = time.monotonic()
        result: dict[str, object] = {
            "memory": label, "command": command, "serial_log": str(serial_log),
        }
        with serial_log.open("wb") as log:
            process = subprocess.Popen(command, stdin=subprocess.PIPE,
                                       stdout=log, stderr=subprocess.STDOUT,
                                       env=env, start_new_session=True)
            try:
                deadline = started + args.timeout
                saw_userspace = False
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        break
                    try:
                        output_text = serial_log.read_text(errors="replace")
                    except FileNotFoundError:
                        output_text = ""
                    if "Kernel panic" in output_text or "invalid RAM size" in output_text:
                        break
                    if "BusyBox v" in output_text and re.search(r"\n#\s", output_text):
                        expected_range = f"0x{0x101000:08x}-0x{byte_count:08x}"
                        expected_pages = byte_count // 4096 - ((0x101000 + 4095) // 4096)
                        expected_memory = f"{expected_pages:,}".replace(",", "")
                        result["reported_ram"] = expected_range in output_text
                        result["reported_pages"] = f"Total pages: {expected_memory}" in output_text
                        result["userspace_shell"] = True
                        assert process.stdin is not None
                        prior_output_len = len(output_text)
                        process.stdin.write(b"/bin/busybox true; echo $HOME\n")
                        process.stdin.flush()
                        while time.monotonic() < deadline:
                            output_text = serial_log.read_text(errors="replace")
                            command_output = output_text[prior_output_len:]
                            if re.search(r"\r?\n/root\r?\n", command_output):
                                result["userspace_smoke"] = True
                                break
                            if process.poll() is not None:
                                break
                            time.sleep(0.1)
                        saw_userspace = True
                        break
                    time.sleep(0.1)

                result["userspace_shell"] = saw_userspace
                if saw_userspace and not result.get("userspace_smoke"):
                    result["error"] = "Shell booted but could not run the guest RAM smoke command"
                    result["failure_tail"] = serial_log.read_text(errors="replace")[-3000:]
                    overall_ok = False
                result["duration_seconds"] = round(time.monotonic() - started, 2)
                if saw_userspace:
                    qmp_quit(qmp_socket)
                    process.wait(timeout=10)
                else:
                    result["failure_tail"] = serial_log.read_text(errors="replace")[-3000:]
                    overall_ok = False
            except Exception as error:
                result["error"] = f"{type(error).__name__}: {error}"
                overall_ok = False
            finally:
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                qmp_socket.unlink(missing_ok=True)

        if saw_userspace and not (result.get("reported_ram") and result.get("reported_pages")):
            result["error"] = "Shell reached, but discovered RAM/page count did not match"
            result["failure_tail"] = serial_log.read_text(errors="replace")[-3000:]
            overall_ok = False
        result["pass"] = bool(saw_userspace and result.get("reported_ram") and
                               result.get("reported_pages") and result.get("userspace_smoke"))
        print(f"{label}: {'PASS' if result['pass'] else 'FAIL'}", flush=True)
        report["results"].append(result)  # type: ignore[union-attr]
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")

    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
