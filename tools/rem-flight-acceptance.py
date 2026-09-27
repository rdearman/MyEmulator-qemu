#!/usr/bin/env python3
"""End-to-end acceptance checks for an explicitly pinned REM flight tuple.

The guest disk is copied to an isolated temporary directory before overlays
or boot. Reports and serial transcripts remain there for diagnosis.
"""

from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import pty
import re
import secrets
import select
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time
import ipaddress
from typing import Any
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TUPLE = ROOT / "tools/rem-flight-image/tuples/flight-20260925.json"
DEFAULT_OVERLAYS = ROOT / "tools/rem-flight-acceptance/artifacts.json"
PHASES = (
    "ARTIFACT IDENTITY", "BOOT", "CONSOLE INPUT", "SHELL", "FILESYSTEM",
    "YASH", "KILO", "VIM", "CHIBICC", "RUNTIME ABI", "MAKE",
    "BINUTILS ARCHIVE", "SAMURAI", "APPLICATION TORTURE", "NETWORK", "PERSISTENCE",
)
PROMPT = re.compile(rb"(?:^|\r*\n|\x1b\[\?1049l)# ")
FATAL = re.compile(
    rb"Kernel panic|Attempted to kill init|Failed to execute /sbin/init|"
    rb"EXT4-fs error|MYEMU_BAD_USER_FAULT|MYEMU llist bad add|"
    rb"QEMU:.*(error|failed)|qemu-system-myemulator32:.*(error|failed)", re.I
)
GUEST_HASH_PATHS = (
    "/sbin/init", "/bin/busybox", "/bin/bash",
    "/root/native/bin/as", "/root/native/bin/ld",
    "/usr/bin/kilo", "/bin/kilo", "/usr/local/bin/kilo", "/usr/bin/vim",
    "/usr/bin/yash", "/bin/yash", "/usr/local/bin/yash",
    "/root/native/bin/chibicc", "/root/native/bin/samu", "/usr/bin/samu",
    "/bin/samu", "/root/samu", "/usr/bin/make", "/bin/make",
    "/usr/bin/grep", "/bin/grep", "/usr/bin/sed", "/bin/sed",
    "/usr/bin/tar", "/bin/tar", "/usr/bin/less", "/bin/less",
    "/usr/bin/dialog", "/bin/dialog", "/usr/bin/git", "/bin/git",
    "/usr/bin/sort", "/bin/sort", "/usr/bin/uniq", "/bin/uniq",
    "/usr/bin/wc", "/bin/wc",
)


class AcceptanceError(RuntimeError):
    pass


REQUIRED_FLIGHT_TOOLS = (
    "yash", "kilo", "chibicc", "as", "ld", "ar", "ranlib",
    "libc", "libgcc", "crt1", "crti", "crtn",
)
CANONICAL_LINK_PATHS = {
    "chibicc": "/root/native/bin/chibicc",
    "as": "/root/native/bin/as",
    "ld": "/root/native/bin/ld",
    "ar": "/root/native/bin/ar",
    "ranlib": "/root/native/bin/ranlib",
    "linker_script": "/root/native/lib/myemulator2-user.ld",
    "libc": "/usr/lib/libc.a",
    "libgcc": "/root/native/lib/libgcc.a",
    "crt1": "/usr/lib/crt1.o",
    "crti": "/usr/lib/crti.o",
    "crtn": "/usr/lib/crtn.o",
    "make": "/usr/bin/make",
    "vim": "/usr/bin/vim",
}


def validate_candidate_gate(meta: dict[str, Any]) -> None:
    """Require an explicit unified-candidate lock before any VM is started."""
    gate = meta.get("acceptance_gate")
    if not isinstance(gate, dict) or gate.get("unified") is not True:
        raise AcceptanceError("candidate is not marked unified in acceptance_gate.unified")
    tools = gate.get("tools")
    if not isinstance(tools, dict):
        raise AcceptanceError("unified candidate has no acceptance_gate.tools lock")
    guest_artifacts = {
        item.get("path"): item for item in meta.get("guest_artifacts", [])
        if isinstance(item, dict) and item.get("path")
    }
    roles = REQUIRED_FLIGHT_TOOLS + ("linker_script",)
    roles += tuple(role for role in ("make", "samu", "vim") if role in tools)
    for role in roles:
        item = tools.get(role)
        if not isinstance(item, dict):
            raise AcceptanceError(f"unified candidate does not pin required tool: {role}")
        path = item.get("path")
        digest = item.get("sha256")
        if role in CANONICAL_LINK_PATHS and path != CANONICAL_LINK_PATHS[role]:
            raise AcceptanceError(
                f"stale/noncanonical {role} path: expected {CANONICAL_LINK_PATHS[role]}, got {path}"
            )
        if not isinstance(path, str) or not path.startswith("/") or ".." in PurePosixPath(path).parts:
            raise AcceptanceError(f"invalid guest path for flight tool {role}: {path}")
        if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise AcceptanceError(f"missing/invalid SHA-256 for flight tool {role}")
        if item.get("validated") is not True:
            raise AcceptanceError(f"refusing unvalidated flight artifact: {role} ({path})")
        recorded = guest_artifacts.get(path)
        if not recorded or recorded.get("sha256") != digest:
            raise AcceptanceError(f"tool hash/path does not match pinned guest artifact: {role} ({path})")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_capture(argv: list[str], *, timeout: int = 60) -> subprocess.CompletedProcess[str]:
    return subprocess.run(argv, text=True, capture_output=True, timeout=timeout, check=False)


def parse_debugfs_mode(output: str) -> str | None:
    match = re.search(r"\bMode:\s+(\S+)", output)
    if not match or not re.fullmatch(r"[0-7]{3,4}", match.group(1)):
        return None
    value = int(match.group(1), 8)
    if value > 0o7777:
        return None
    return format(value, "04o")


def parse_artifacts(args: argparse.Namespace) -> tuple[dict[str, Any], dict[str, Path], dict[str, str]]:
    if args.candidate_dir:
        base = args.candidate_dir.resolve(strict=True)
        manifest_path = base / "manifest.json"
        if not manifest_path.is_file():
            raise AcceptanceError(f"candidate has no manifest.json: {base}")
        manifest = json.loads(manifest_path.read_text())
        validate_candidate_gate(manifest)
        inputs = manifest.get("inputs", {})
        outputs = manifest.get("outputs", {})
        files = {
            "qemu": base / "qemu-system-myemulator32",
            "kernel": base / "vmlinux",
            "rootfs": base / "rem-rootfs.ext4",
        }
        expected = {
            "qemu": inputs.get("qemu", {}).get("sha256"),
            "kernel": inputs.get("kernel", {}).get("sha256"),
            "rootfs": outputs.get("rootfs", {}).get("sha256"),
        }
        tuple_meta = {k: manifest.get(k) for k in ("tuple", "machine", "memory", "icount")}
        tuple_meta["name"] = manifest.get("tuple", base.name)
        tuple_meta["guest_artifacts"] = manifest.get("guest_artifacts", [])
        tuple_meta["required_guest_files"] = manifest.get("required_guest_files", [])
        tuple_meta["acceptance_gate"] = manifest.get("acceptance_gate", {})
        return tuple_meta, files, expected

    tuple_path = args.tuple.resolve(strict=True)
    spec = json.loads(tuple_path.read_text())
    validate_candidate_gate(spec)
    files: dict[str, Path] = {}
    expected: dict[str, str] = {}
    path_root = ROOT
    for key in ("qemu", "kernel", "rootfs"):
        item = spec.get("inputs", {}).get(key)
        if not item or not item.get("path") or not item.get("sha256"):
            raise AcceptanceError(f"tuple must pin path and SHA-256 for {key}: {tuple_path}")
        p = Path(item["path"])
        files[key] = p.resolve() if p.is_absolute() else (path_root / p).resolve()
        expected[key] = item["sha256"]
    tuple_meta = dict(spec)
    tuple_meta["tuple_file"] = str(tuple_path)
    return tuple_meta, files, expected


def validate_artifacts(files: dict[str, Path], expected: dict[str, str],
                       meta: dict[str, Any]) -> dict[str, Any]:
    identities: dict[str, Any] = {}
    for key in ("qemu", "kernel", "rootfs"):
        path = files[key]
        if not path.is_file():
            raise AcceptanceError(f"pinned {key} artifact is missing: {path}")
        actual = sha256(path)
        wanted = expected.get(key)
        if not wanted:
            raise AcceptanceError(f"manifest does not pin the tested {key} SHA-256")
        if actual != wanted:
            raise AcceptanceError(f"{key} SHA-256 mismatch for {path}: expected {wanted}, got {actual}")
        identities[key] = {"path": str(path), "sha256": actual, "size": path.stat().st_size}
    for key in ("qemu", "kernel"):
        info = run_capture(["readelf", "-h", str(files[key])])
        if info.returncode:
            raise AcceptanceError(f"readelf failed for {key}: {info.stderr.strip()}")
        match = re.search(r"^\s*Machine:\s*(.*?)\s*$", info.stdout, re.M)
        identities[key]["elf_machine"] = match.group(1) if match else None
    if identities["qemu"]["elf_machine"] != "Advanced Micro Devices X86-64":
        raise AcceptanceError("host acceptance requires the pinned x86-64 QEMU executable")
    identities["machine"] = meta.get("machine", "myemulator32")
    identities["memory"] = meta.get("memory", "128M")
    identities["icount"] = meta.get("icount", "shift=0,sleep=off")
    return identities


def check_guest_file(image: Path, guest_path: str) -> dict[str, Any] | None:
    result = run_capture(["debugfs", "-R", f"stat {guest_path}", str(image)])
    if result.returncode or "Type: regular" not in result.stdout:
        return None
    size_match = re.search(r"\bSize:\s*(\d+)", result.stdout)
    mode = parse_debugfs_mode(result.stdout)
    if not size_match or mode is None:
        return None
    with tempfile.TemporaryDirectory(prefix="rem-accept-extract-") as temp:
        extracted = Path(temp) / "guest-file"
        dumped = run_capture(["debugfs", "-R", f"dump {guest_path} {extracted}", str(image)])
        if dumped.returncode or not extracted.is_file():
            return None
        return {
            "path": guest_path, "size": extracted.stat().st_size,
            "sha256": sha256(extracted),
            "mode": mode,
        }


def verify_guest_image(image: Path, meta: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    report: dict[str, Any] = {"files": {}, "manifest_guest_artifacts": []}
    errors: list[str] = []
    fsck = run_capture(["e2fsck", "-fn", str(image)], timeout=180)
    report["e2fsck_returncode"] = fsck.returncode
    report["e2fsck_output"] = (fsck.stdout + fsck.stderr)[-4000:]
    if fsck.returncode != 0:
        errors.append(f"e2fsck -fn returned {fsck.returncode}")
    must_exist = meta.get("required_guest_files") or ["/sbin/init", "/bin/busybox"]
    for guest_path in must_exist:
        found = check_guest_file(image, guest_path)
        if not found or found["size"] <= 0:
            errors.append(f"required guest file missing, truncated, or empty: {guest_path}")
    for guest_path in GUEST_HASH_PATHS:
        found = check_guest_file(image, guest_path)
        if found:
            report["files"][guest_path] = found
    for item in meta.get("guest_artifacts", []):
        guest_path = item.get("path")
        if not guest_path or not guest_path.startswith("/") or ".." in PurePosixPath(guest_path).parts:
            errors.append(f"unsafe guest artifact path in manifest: {guest_path}")
            continue
        found = check_guest_file(image, guest_path)
        if not found:
            errors.append(f"manifest guest file missing: {guest_path}")
            continue
        report["manifest_guest_artifacts"].append(found)
        if found["size"] != item.get("size") or found["sha256"] != item.get("sha256"):
            errors.append(f"manifest guest artifact size/hash mismatch: {guest_path}")
        if item.get("mode") and int(found["mode"] or "0", 8) != int(item["mode"], 8):
            errors.append(f"manifest guest artifact mode mismatch: {guest_path}")
    return report, errors


def apply_overlays(image: Path, manifest_path: Path) -> list[dict[str, Any]]:
    data = json.loads(manifest_path.read_text())
    applied = []
    for item in data.get("artifacts", []):
        if item.get("accepted") is not True:
            raise AcceptanceError(f"refusing unaccepted overlay artifact: {item.get('name', item)}")
        guest_path = item.get("guest_path")
        if (not guest_path or not guest_path.startswith("/") or
                ".." in PurePosixPath(guest_path).parts or '"' in guest_path):
            raise AcceptanceError(f"invalid overlay destination: {guest_path}")
        source = Path(item["source"])
        if not source.is_absolute():
            source = ROOT / source
        source = source.resolve(strict=True)
        actual = sha256(source)
        if actual != item.get("sha256"):
            raise AcceptanceError(f"overlay hash mismatch for {item.get('name')}: expected {item.get('sha256')}, got {actual}")
        existing = run_capture(["debugfs", "-R", f"stat {guest_path}", str(image)])
        if existing.returncode == 0 and "Type: regular" in existing.stdout:
            applied.append({"name": item.get("name", guest_path), "host_path": str(source),
                            "host_sha256": actual, "guest_path": guest_path,
                            "size": source.stat().st_size, "already_integrated": True})
            continue
        write = run_capture(["debugfs", "-w", "-R", f"write {source} {guest_path}", str(image)])
        if write.returncode:
            raise AcceptanceError(f"debugfs could not stage {guest_path}: {write.stderr.strip()} {write.stdout.strip()}")
        applied.append({"name": item.get("name", guest_path), "host_path": str(source),
                        "host_sha256": actual, "guest_path": guest_path,
                        "size": source.stat().st_size})
    return applied


class Guest:
    def __init__(self, argv: list[str], log_path: Path, timeout: int):
        self.argv = argv
        self.log_path = log_path
        self.timeout = timeout
        self.master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
        self.proc = subprocess.Popen(argv, stdin=slave, stdout=slave, stderr=slave,
                                     start_new_session=True, close_fds=True)
        os.close(slave)
        os.set_blocking(self.master, False)
        self.buffer = bytearray()
        self.log = log_path.open("wb")
        self.started = time.monotonic()

    def sendline(self, value: str) -> None:
        os.write(self.master, value.encode("utf-8", "replace") + b"\n")

    def expect(self, pattern: bytes | re.Pattern[bytes], timeout: float | None = None) -> bytes:
        compiled = re.compile(pattern) if isinstance(pattern, bytes) else pattern
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        while time.monotonic() < deadline:
            fatal = FATAL.search(self.buffer)
            if fatal:
                raise AcceptanceError("fatal kernel/filesystem/QEMU diagnostic: " +
                                      fatal.group(0).decode("latin1", "replace"))
            match = compiled.search(self.buffer)
            if match:
                result = bytes(self.buffer[:match.end()])
                del self.buffer[:match.end()]
                return result
            if self.proc.poll() is not None:
                raise AcceptanceError(f"QEMU exited unexpectedly with status {self.proc.returncode}")
            ready, _, _ = select.select([self.master], [], [], min(0.2, deadline - time.monotonic()))
            if ready:
                try:
                    data = os.read(self.master, 65536)
                except OSError as exc:
                    raise AcceptanceError(f"serial PTY read failed: {exc}") from exc
                if not data:
                    raise AcceptanceError("serial PTY closed before expected guest response")
                self.log.write(data)
                self.log.flush()
                self.buffer.extend(data)
        tail = bytes(self.buffer[-2000:]).decode("latin1", "replace")
        raise TimeoutError(f"timed out waiting for {compiled.pattern!r}; pending serial: {tail}")

    def command(self, command: str, marker: str, timeout: float | None = None) -> str:
        self.sendline(f"{command}; rc=$?; echo {marker}:$rc")
        result = self.expect(re.compile(rb"\r*\n" + marker.encode() + rb":([0-9]+)\r*\n"), timeout)
        match = re.search(rb"\r*\n" + marker.encode() + rb":([0-9]+)\r*\n", result)
        if not match:
            raise AcceptanceError(f"guest command did not return marker {marker}")
        if match.group(1) != b"0":
            status = match.group(1).decode()
            self.expect(PROMPT, timeout)
            raise AcceptanceError(f"guest command returned {status}: {command}")
        self.expect(PROMPT, timeout)
        return result.decode("latin1", "replace")

    def stop(self) -> None:
        if self.proc.poll() is None:
            try:
                os.killpg(self.proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(self.proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                self.proc.wait(timeout=5)
        self.log.close()
        try:
            os.close(self.master)
        except OSError:
            pass


def clean_text(data: str) -> str:
    text = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", data)
    return text.replace("\r", "")


def parse_guest_hashes(output: str) -> dict[str, str]:
    hashes: dict[str, str] = {}
    for line in clean_text(output).splitlines():
        match = re.search(r"\b([0-9a-f]{64})\s+(/[^\s]+)\s*$", line)
        if match:
            hashes[match.group(2)] = match.group(1)
    return hashes


def boot_guest(argv: list[str], log_path: Path, timeout: int) -> Guest:
    guest = Guest(argv, log_path, timeout)
    try:
        guest.expect(re.compile(rb"VFS: Mounted root \(ext4 filesystem\)"), timeout)
        guest.expect(re.compile(rb"Run /sbin/init as init process"), timeout)
        guest.expect(PROMPT, timeout)
        # Prevent PTY line echo from making an input marker look like program
        # output. All subsequent nonce checks then observe guest responses.
        guest.command("stty -echo", "REM_ACCEPT_DISABLE_ECHO", min(timeout, 8))
        return guest
    except Exception:
        guest.stop()
        raise


def guest_app_path(image: Path, paths: list[str]) -> str | None:
    for path in paths:
        if check_guest_file(image, path):
            return path
    return None


def run_kilo(guest: Guest, path: str | None, nonce: int) -> tuple[str, str]:
    if not path:
        return "NOT INSTALLED", "No Kilo executable is installed in the selected flight rootfs."
    filename = f"/root/rem-kilo-accept-{nonce}.txt"
    token = f"KILO_ACCEPT_{nonce}"
    try:
        guest.sendline(f"export TERM=linux; : > {filename}; {path} {filename}")
        # The editor command does not return until save and quit. Wait for its
        # on-screen key legend before sending text.
        guest.expect(re.compile(rb"(?i)ctrl.?s.{0,80}ctrl.?q|ctrl.?q.{0,80}ctrl.?s"), 8)
        os.write(guest.master, token.encode())
        guest.expect(re.escape(token.encode()), 5)
        os.write(guest.master, b"\x13")  # Ctrl-S saves.
        time.sleep(0.15)
        os.write(guest.master, b"\x11")  # Ctrl-Q exits.
        guest.expect(PROMPT, 8)
        output = guest.command(f"cat {filename}", f"KILO_READ_{nonce}")
        if token not in clean_text(output):
            raise AcceptanceError("saved Kilo file did not contain its unique edit token")
        return "PASS", "Kilo opened a new file, saved typed contents, exited, and the file read back exactly."
    except (AcceptanceError, TimeoutError) as exc:
        return "FAIL", str(exc)


def run_vim(guest: Guest, path: str, nonce: int) -> tuple[str, str]:
    filename = f"/root/rem-vim-accept-{nonce}.txt"
    token = f"VIM_EDIT_SAVE_{nonce}"
    try:
        version = guest.command(f"{path} --version", f"VIM_VERSION_{nonce}", timeout=12)
        if "VIM - Vi IMproved" not in clean_text(version):
            raise AcceptanceError("Vim startup/version output was not recognized")
        guest.command(
            f"{path} -u NONE -i NONE -n -es -c \"call setline(1, '{token}')\" "
            f"-c \"wq\" {filename}", f"VIM_EDIT_{nonce}", timeout=20)
        saved = guest.command(f"cat {filename}", f"VIM_READ_{nonce}")
        if clean_text(saved).splitlines().count(token) != 1:
            raise AcceptanceError("Vim saved buffer did not read back exactly once")
        return "PASS", "Vim started, edited a new buffer, saved it, and the saved line matched exactly."
    except (AcceptanceError, TimeoutError) as exc:
        return "FAIL", str(exc)


def run_yash(guest: Guest, path: str | None, nonce: int) -> tuple[str, str]:
    if not path:
        return "PENDING OWNER", "No accepted Yash binary is integrated in the selected flight rootfs."
    prompt = f"YASH_ACCEPT_{nonce}>"
    prefix = f"env \"PS1=YASH_ACCEPT_{nonce}\"'>' {path} -i"
    try:
        guest.sendline(prefix)
        guest.expect(re.escape(prompt.encode()), 10)

        def yash_command(command: str, expected: str, mark: str) -> None:
            guest.sendline(command)
            output = guest.expect(re.compile(re.escape(expected.encode()) + rb"\r*\n"), 6)
            if expected.encode() not in output:
                raise AcceptanceError(f"Yash did not return expected {mark} value")
            guest.expect(re.escape(prompt.encode()), 6)

        execution = f"YASH_EXEC_{nonce}"
        yash_command(f"YASH_EXEC_PREFIX=YASH_EXEC_; YASH_EXEC_SUFFIX={nonce}; echo \"$YASH_EXEC_PREFIX$YASH_EXEC_SUFFIX\"",
                     execution, "command execution")
        pipeline = f"YASH_PIPE_{nonce}"
        yash_command(f"YASH_PIPE_PREFIX=YASH_PIPE_; YASH_PIPE_SUFFIX={nonce}; echo \"$YASH_PIPE_PREFIX$YASH_PIPE_SUFFIX\" | /bin/cat",
                     pipeline, "pipeline")
        redirected = f"YASH_REDIRECT_{nonce}"
        file = f"/tmp/rem-yash-redir-{nonce}"
        yash_command(f"YASH_REDIRECT_PREFIX=YASH_REDIRECT_; YASH_REDIRECT_SUFFIX={nonce}; echo \"$YASH_REDIRECT_PREFIX$YASH_REDIRECT_SUFFIX\" > {file}; cat {file}",
                     redirected, "redirection")
        env_value = f"YASH_ENV_{nonce}"
        yash_command(f"YASH_ACCEPT_PREFIX=YASH_ENV_; YASH_ACCEPT_VALUE=\"$YASH_ACCEPT_PREFIX{nonce}\"; export YASH_ACCEPT_VALUE; echo \"$YASH_ACCEPT_VALUE\"",
                     env_value, "environment")
        child_value = f"YASH_CHILD_{nonce}"
        yash_command(f"env YASH_CHILD_PREFIX=YASH_CHILD_ YASH_CHILD_SUFFIX={nonce} /bin/busybox sh -c 'echo \"$YASH_CHILD_PREFIX$YASH_CHILD_SUFFIX\"'",
                     child_value, "child process")
        guest.sendline("exit")
        guest.expect(re.compile(rb"REM shell exited|child shell exited"), 6)
        guest.expect(PROMPT, 6)
        return "PASS", "Interactive prompt, command, pipeline, redirection, environment, child process, and clean exit passed."
    except (AcceptanceError, TimeoutError) as exc:
        return "FAIL", str(exc)


def run_samurai(guest: Guest, path: str | None, nonce: int, compiler_present: bool) -> tuple[str, str]:
    if not path:
        return "PENDING OWNER", "No accepted Samurai executable is integrated in the selected flight rootfs."
    if not compiler_present:
        return "PENDING OWNER", "Samurai is installed but accepted chibicc is unavailable for the build edge."
    source = f"/root/samu-accept-{nonce}.c"
    ninja = f"/root/samu-accept-{nonce}.ninja"
    executable = f"/root/samu-accept-{nonce}"
    first_token = f"SAMURAI_FIRST_{nonce}"
    second_token = f"SAMURAI_SECOND_{nonce}"
    try:
        help_output = guest.command(f"{path} -h", f"SAMURAI_HELP_{nonce}", timeout=8)
        if not re.search(r"(?i)usage|options|samu", clean_text(help_output)):
            raise AcceptanceError("samu -h exited but did not print recognizable help text")
        guest.command(
            f"echo 'int puts(const char *);' > {source}; "
            f"echo 'int main(void) {{ puts(\"SAMURAI_\" \"FIRST_{nonce}\"); return 0; }}' >> {source}; "
            f"echo 'rule cc' > {ninja}; "
            f"echo ' command=env CHIBICC_REM_AS=/root/native/bin/as CHIBICC_REM_LD=/root/native/bin/ld CHIBICC_REM_LINKER_SCRIPT=/root/native/lib/myemulator2-user.ld CHIBICC_REM_CRTDIR=/usr/lib CHIBICC_REM_GCCLIB=/root/native/lib /root/native/bin/chibicc -o $out $in' >> {ninja}; "
            f"echo 'build {executable}: cc {source}' >> {ninja}; "
            f"echo 'default {executable}' >> {ninja}",
            f"SAMURAI_SETUP_{nonce}")
        dry = guest.command(f"{path} -n -f {ninja}", f"SAMURAI_DRY_{nonce}", timeout=12)
        if "/root/native/bin/chibicc" not in clean_text(dry):
            raise AcceptanceError("samu -n did not print the pinned compiler build edge")
        guest.command(f"{path} -f {ninja}", f"SAMURAI_BUILD1_{nonce}", timeout=25)
        first = guest.command(executable, f"SAMURAI_RUN1_{nonce}")
        if first_token not in clean_text(first):
            raise AcceptanceError("Samurai-built executable did not print first source result")
        time.sleep(1.1)
        guest.command(
            f"echo 'int puts(const char *);' > {source}; "
            f"echo 'int main(void) {{ puts(\"SAMURAI_\" \"SECOND_{nonce}\"); return 0; }}' >> {source}",
            f"SAMURAI_EDIT_{nonce}")
        incremental = guest.command(f"{path} -n -f {ninja}", f"SAMURAI_INCREMENTAL_{nonce}", timeout=12)
        if "/root/native/bin/chibicc" not in clean_text(incremental):
            raise AcceptanceError("samu dry run did not schedule rebuild after source modification")
        guest.command(f"{path} -f {ninja}", f"SAMURAI_BUILD2_{nonce}", timeout=25)
        second = guest.command(executable, f"SAMURAI_RUN2_{nonce}")
        if second_token not in clean_text(second):
            raise AcceptanceError("incrementally rebuilt executable did not print updated source result")
        return "PASS", "samu -n planned the compiler edge, samu built and ran the executable, then a source edit caused a verified incremental rebuild."
    except (AcceptanceError, TimeoutError) as exc:
        return "FAIL", str(exc)


def run_ar_ranlib(guest: Guest, nonce: int) -> tuple[str, str]:
    """Prove archive creation/indexing and linking using the pinned native tools."""
    work = f"/root/rem-archive-{nonce}"
    lib_c = f"{work}/member.c"
    main_c = f"{work}/main.c"
    try:
        guest.command(
            f"mkdir -p {work}; "
            f"echo 'int rem_answer(void) {{ return 42; }}' > {lib_c}; "
            f"echo 'int rem_answer(void); int puts(const char *); int main(void) {{ if (rem_answer() != 42) return 1; puts(\"REM_ARCHIVE_{nonce}\"); return 0; }}' > {main_c}; "
            f"/root/native/bin/chibicc -S -o {work}/member.s {lib_c} && "
            f"/root/native/bin/chibicc -S -o {work}/main.s {main_c}",
            f"ARCHIVE_PREP_{nonce}")
        guest.command(
            f"/root/native/bin/as -o {work}/member.o {work}/member.s && "
            f"/root/native/bin/as -o {work}/main.o {work}/main.s",
            f"ARCHIVE_AS_{nonce}")
        guest.command(
            f"cd {work} && /root/native/bin/ar rcs librem.a member.o && "
            f"/root/native/bin/ranlib librem.a && "
            f"/root/native/bin/ar t librem.a > members.txt && grep -x member.o members.txt",
            f"ARCHIVE_TOOLS_{nonce}")
        guest.command(
            f"/root/native/bin/ld -T /root/native/lib/myemulator2-user.ld "
            f"-o {work}/archive-test /usr/lib/crt1.o /usr/lib/crti.o {work}/main.o "
            f"--start-group {work}/librem.a --end-group /usr/lib/libc.a "
            f"/root/native/lib/libgcc.a /usr/lib/crtn.o",
            f"ARCHIVE_LINK_{nonce}")
        result = guest.command(f"{work}/archive-test", f"ARCHIVE_RUN_{nonce}")
        if f"REM_ARCHIVE_{nonce}" not in clean_text(result):
            raise AcceptanceError("archive-linked executable output did not match its nonce")
        return "PASS", "ar created/listed the archive, ranlib indexed it, canonical ld linked it, and the executable returned the expected nonce."
    except (AcceptanceError, TimeoutError) as exc:
        return "FAIL", str(exc)


def run_make(guest: Guest, nonce: int) -> tuple[str, str]:
    """Test make's build and timestamp-driven rebuild behavior with C output."""
    work = f"/root/rem-make-{nonce}"
    source = f"{work}/hello.c"
    makefile = f"{work}/Makefile"
    executable = f"{work}/hello"
    build_rule = (
        "all: hello\n\n"
        "hello: hello.c\n"
        "\tenv CHIBICC_REM_AS=/root/native/bin/as CHIBICC_REM_LD=/root/native/bin/ld "
        "CHIBICC_REM_LINKER_SCRIPT=/root/native/lib/myemulator2-user.ld "
        "CHIBICC_REM_CRTDIR=/usr/lib CHIBICC_REM_GCCLIB=/root/native/lib "
        "/root/native/bin/chibicc -o hello hello.c\n"
    )
    try:
        guest.command(f"mkdir -p {work}", f"MAKE_DIR_{nonce}")
        guest.command(
            f"cat > {source} <<'REM_C'\nint puts(const char *); int main(void) {{ puts(\"MAKE_FIRST_{nonce}\"); return 0; }}\nREM_C\n"
            f"cat > {makefile} <<'REM_MAKE'\n{build_rule}REM_MAKE\n"
            f"cd {work} && /usr/bin/make -f Makefile",
            f"MAKE_BUILD1_{nonce}", timeout=30)
        first = guest.command(executable, f"MAKE_RUN1_{nonce}")
        if f"MAKE_FIRST_{nonce}" not in clean_text(first):
            raise AcceptanceError("make-built executable did not print first source output")
        dryrun = guest.command(
            f"sleep 1; cat > {source} <<'REM_C'\nint puts(const char *); int main(void) {{ puts(\"MAKE_EDITED_{nonce}\"); return 0; }}\nREM_C\n"
            f"cd {work} && /usr/bin/make -n -f Makefile",
            f"MAKE_DRYRUN_{nonce}", timeout=15)
        if "/root/native/bin/chibicc" not in clean_text(dryrun):
            raise AcceptanceError("make dry-run did not schedule the compiler after source edit")
        guest.command(f"cd {work} && /usr/bin/make -f Makefile", f"MAKE_BUILD2_{nonce}", timeout=30)
        second = guest.command(executable, f"MAKE_RUN2_{nonce}")
        if f"MAKE_EDITED_{nonce}" not in clean_text(second):
            raise AcceptanceError("incrementally rebuilt make executable did not reflect source edit")
        return "PASS", "make built and ran C output, source was edited, dry-run scheduled rebuild, and rebuilt executable printed the changed output."
    except (AcceptanceError, TimeoutError) as exc:
        return "FAIL", str(exc)


def run_runtime_contract(guest: Guest, nonce: int, source: str) -> tuple[str, str]:
    """Compile the immutable host fixture and report ABI/runtime subchecks."""
    assembly = f"/root/rem-runtime-contract-{nonce}.s"
    obj = f"/root/rem-runtime-contract-{nonce}.o"
    executable = f"/root/rem-runtime-contract-{nonce}"
    try:
        guest.command(
            f"/root/native/bin/chibicc -S -o {assembly} {source} && "
            f"/root/native/bin/as -o {obj} {assembly} && "
            f"/root/native/bin/ld -T /root/native/lib/myemulator2-user.ld "
            f"-o {executable} /usr/lib/crt1.o /usr/lib/crti.o {obj} "
            f"/usr/lib/libc.a /root/native/lib/libgcc.a /usr/lib/crtn.o",
            f"RUNTIME_BUILD_{nonce}", timeout=40)
        output = guest.command(f"{executable} alpha beta", f"RUNTIME_RUN_{nonce}", timeout=20)
        expected = (
            "RUNTIME_PASS_ARGV", "RUNTIME_PASS_VARIADIC", "RUNTIME_PASS_POINTERS",
            "RUNTIME_PASS_INT64",
        )
        missing = [token for token in expected if token not in clean_text(output)]
        if missing:
            layer = missing[0].removeprefix("RUNTIME_PASS_").lower()
            raise AcceptanceError(f"runtime contract did not reach {missing[0]} (failure layer: {layer})")
        return "PASS", "Native executable validated argv, variadic arguments, pointer tables, and 64-bit arithmetic with all exact markers."
    except (AcceptanceError, TimeoutError) as exc:
        message = str(exc)
        failure_layers = {
            "11": "execve/argc/argv ABI", "12": "variadic ABI/stdarg",
            "13": "pointer table/data relocation", "14": "64-bit arithmetic",
        }
        for exit_code, layer in failure_layers.items():
            if f"returned {exit_code}:" in message:
                message += f" (failure layer: {layer})"
                break
        return "FAIL", message


def compiler_provenance(meta: dict[str, Any], guest_path: str) -> str:
    for item in meta.get("guest_artifacts", []):
        if item.get("path") == guest_path:
            for key in ("compiler_used", "compiler", "built_with"):
                if item.get(key):
                    return str(item[key])
            if item.get("provenance"):
                return f"provenance recorded, compiler unspecified: {item['provenance']}"
    return "UNKNOWN: candidate manifest does not record compiler provenance"


def fault_pc_from_log(log_path: Path) -> str | None:
    try:
        output = log_path.read_text(errors="replace")
    except OSError:
        return None
    patterns = (
        r"\b(?:PC|pc)\s*(?:=|:)\s*(0x[0-9a-fA-F]+)",
        r"(?:MYEMU_[A-Z_]+[^\n]*?)(0x[0-9a-fA-F]{4,})",
    )
    for pattern in patterns:
        match = re.search(pattern, output)
        if match:
            return match.group(1)
    return None


def run_torture_case(guest: Guest, definition: dict[str, Any], nonce: int,
                     meta: dict[str, Any]) -> dict[str, Any]:
    test_id = definition["id"]
    candidates = definition.get("binary_candidates", [])
    item: dict[str, Any] = {
        "id": test_id, "tier": definition.get("tier"),
        "subsystem": definition.get("subsystem"),
        "expected_result": definition.get("expected_result", definition.get("expected")),
        "failure_layer": definition.get("subsystem"),
        "binary_hashes": {}, "binary_hash_sources": {}, "compiler_used": {},
        "status": "TEST ERROR", "fault_pc": None,
        "serial_log": str(guest.log_path),
    }
    first_binary = candidates[0] if candidates else ""
    item["command"] = definition.get("command", "").format(n=nonce, bin=first_binary)
    item["minimal_reproducer"] = definition.get("minimal_reproducer", item["command"]).format(n=nonce, bin=first_binary)
    verified_guest_files = meta.get("verified_guest_files", {})
    for candidate in candidates:
        offline = verified_guest_files.get(candidate, {}).get("sha256")
        item["binary_hashes"][candidate] = offline
        item["binary_hash_sources"][candidate] = "offline guest image verification" if offline else "unavailable"
        item["compiler_used"][candidate] = compiler_provenance(meta, candidate)
    try:
        selected: list[str] = []
        for candidate in candidates:
            probe = guest.command(
                f"test -x {candidate} && echo TORTURE_PRESENT || echo TORTURE_MISSING",
                f"TORTURE_PROBE_{nonce}_{len(selected)}", timeout=8)
            if "TORTURE_MISSING" in clean_text(probe):
                item.update(status="NOT INSTALLED", detail=f"required binary is absent: {candidate}")
                return item
            selected.append(candidate)
        if not selected:
            item.update(status="TEST ERROR", detail="catalog entry has no binary candidates")
            return item

        for binary in selected:
            item["binary_hashes"][binary] = None
            hash_output = guest.command(f"sha256sum {binary}", f"TORTURE_HASH_{nonce}_{len(item['binary_hashes'])}")
            hashes = parse_guest_hashes(hash_output)
            if binary not in hashes:
                raise AcceptanceError(f"could not obtain SHA-256 for tested binary {binary}")
            offline_hash = item["binary_hashes"].get(binary)
            if offline_hash and offline_hash != hashes[binary]:
                raise AcceptanceError(f"tested binary hash differs from verified image: {binary} expected {offline_hash}, got {hashes[binary]}")
            item["binary_hashes"][binary] = hashes[binary]
            item["binary_hash_sources"][binary] = "guest sha256sum; matched offline image when available"

        binary = selected[0]
        command = definition.get("command", "").format(n=nonce, bin=binary)
        item["command"] = command
        item["minimal_reproducer"] = definition.get("minimal_reproducer", command).format(n=nonce, bin=binary)
        if definition.get("interaction") == "less":
            token = f"REM_LESS_{nonce}"
            guest.command(f"echo {token} > /tmp/rem-less-{nonce}", f"TORTURE_LESS_PREP_{nonce}")
            item["command"] = f"TERM=xterm {binary} -X /tmp/rem-less-{nonce}"
            item["interaction_input"] = "q"
            guest.sendline(f"{item['command']}; rc=$?; echo TORTURE_LESS_RC_{nonce}:$rc")
            visible = guest.expect(re.escape(token.encode()), 12)
            os.write(guest.master, b"q")
            marker = guest.expect(re.compile(rb"TORTURE_LESS_RC_" + str(nonce).encode() + rb":([0-9]+)\r*\n"), 12)
            if re.search(rb":0\r*\n", marker) is None:
                raise AcceptanceError("less did not exit cleanly after the q key")
            guest.expect(PROMPT, 8)
            output = (visible + marker).decode("latin1", "replace")
        elif definition.get("interaction") == "dialog":
            token = f"REM_DIALOG_{nonce}"
            item["command"] = f"TERM=xterm {binary} --clear --msgbox {token} 8 40"
            guest.sendline(f"{item['command']}; rc=$?; echo TORTURE_DIALOG_RC_{nonce}:$rc")
            visible = guest.expect(re.escape(token.encode()), 12)
            os.write(guest.master, b"\r")
            marker = guest.expect(re.compile(rb"TORTURE_DIALOG_RC_" + str(nonce).encode() + rb":([0-9]+)\r*\n"), 12)
            if re.search(rb":0\r*\n", marker) is None:
                raise AcceptanceError("dialog returned a nonzero exit status after Enter")
            guest.expect(PROMPT, 8)
            output = (visible + marker).decode("latin1", "replace")
        else:
            output = guest.command(command, f"TORTURE_{test_id.upper().replace('-', '_')}_{nonce}", timeout=25)

        expected = definition.get("expected", "").format(n=nonce, bin=binary)
        if expected and expected not in clean_text(output):
            raise AcceptanceError(f"expected output not observed: {expected}")
        item.update(status="PASS", detail="expected workload output observed", output=clean_text(output)[-3000:])
        return item
    except (AcceptanceError, TimeoutError, OSError, subprocess.SubprocessError) as exc:
        item["status"] = "FAIL"
        item["detail"] = str(exc)
        item["fault_pc"] = fault_pc_from_log(guest.log_path)
        item["serial_tail"] = guest.log_path.read_text(errors="replace")[-6000:] if guest.log_path.exists() else ""
        return item


def run_network(guest: Guest, meta: dict[str, Any], nonce: int) -> tuple[str, str]:
    net = meta.get("network", {})
    if not net.get("accepted"):
        return "PENDING OWNER", "No accepted network-enabled flight tuple/endpoint is declared (network owner #1)."
    url = net.get("test_url")
    expected = net.get("expected_sha256")
    if not url or not expected:
        return "TEST ERROR", "Network tuple needs test_url and expected_sha256 hooks."
    hostname = urlparse(url).hostname
    try:
        if hostname:
            ipaddress.ip_address(hostname)
            hostname = None
    except ValueError:
        pass
    if not hostname:
        return "TEST ERROR", "Network test_url must use a hostname so DNS resolution is exercised."
    try:
        links = clean_text(guest.command("ip -o link", f"NET_LINKS_{nonce}"))
        interface = None
        for line in links.splitlines():
            match = re.search(r"\d+:\s*([^:@]+)(?:@[^:]+)?:", line)
            if match and match.group(1) != "lo":
                interface = match.group(1)
                break
        if not interface:
            raise AcceptanceError("no non-loopback network interface was listed")
        addresses = clean_text(guest.command(f"ip -4 addr show dev {interface}", f"NET_ADDR_{nonce}"))
        if not re.search(r"inet\s+\d+\.\d+\.\d+\.\d+", addresses):
            raise AcceptanceError(f"{interface} has no IPv4 address")
        routes = clean_text(guest.command("ip route show default", f"NET_ROUTE_{nonce}"))
        if "default" not in routes:
            raise AcceptanceError("no IPv4 default route")
        file = f"/tmp/rem-net-{nonce}.bin"
        guest.command(f"wget -q -O {file} '{url}'", f"NET_DOWNLOAD_{nonce}", timeout=30)
        digest = clean_text(guest.command(f"sha256sum {file}", f"NET_HASH_{nonce}"))
        if expected not in digest:
            raise AcceptanceError(f"downloaded test payload SHA-256 mismatch: {digest.strip()}")
        return "PASS", f"Interface {interface} has IPv4/default route; DNS/TCP/HTTP download hash matched {expected}."
    except (AcceptanceError, TimeoutError) as exc:
        return "FAIL", str(exc)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--tuple", type=Path, default=DEFAULT_TUPLE,
                        help="JSON tuple with pinned input paths and SHA-256 values")
    source.add_argument("--candidate-dir", type=Path,
                        help="flight-image builder output directory with manifest.json")
    parser.add_argument("--overlays", type=Path,
                        help="accepted artifact overlay lock used in the disposable test image")
    parser.add_argument("--results-dir", type=Path, default=Path("/tmp"),
                        help="directory for a unique report/transcript bundle (default: /tmp)")
    parser.add_argument("--timeout", type=int, default=120, help="per-boot/command timeout in seconds")
    parser.add_argument("--repeat", type=int, default=1, help="fresh-copy acceptance iterations")
    args = parser.parse_args()
    if args.overlays is None and not args.candidate_dir:
        args.overlays = DEFAULT_OVERLAYS
    if args.repeat < 1 or args.repeat > 10:
        parser.error("--repeat must be between 1 and 10")

    overall: dict[str, Any] = {
        "status": "TEST ERROR", "repeat_count": args.repeat,
        "phases": {name: {"status": "TEST ERROR", "detail": "not run"} for name in PHASES},
        "iterations": [], "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    try:
        meta, source_files, expected_hashes = parse_artifacts(args)
        identities = validate_artifacts(source_files, expected_hashes, meta)
        overall["tuple"] = meta
        overall["artifacts"] = identities
        catalog_path = ROOT / "tools/rem-flight-acceptance/tests.json"
        runtime_fixture = ROOT / "tools/rem-flight-acceptance/fixtures/runtime-contract.c"
        torture_catalog_path = ROOT / "tools/rem-flight-acceptance/torture-catalog.json"
        catalog = json.loads(catalog_path.read_text())
        torture_catalog = json.loads(torture_catalog_path.read_text())
        gate_tools = meta.get("acceptance_gate", {}).get("tools", {})
        known_hashes = {key: value["sha256"] for key, value in identities.items()
                        if key in ("qemu", "kernel", "rootfs")}
        known_hashes.update({role: item.get("sha256") for role, item in gate_tools.items()
                             if isinstance(item, dict)})
        fixture_hash = sha256(runtime_fixture)
        for definition in catalog.get("tests", []):
            definition["artifact_hashes"] = {
                role: known_hashes.get(role) for role in definition.get("artifact_roles", [])
            }
            if definition.get("phase") == "RUNTIME ABI":
                definition["fixture_sha256"] = fixture_hash
        overall["test_catalog"] = catalog
        overall["test_catalog_sha256"] = sha256(catalog_path)
        overall["application_torture_catalog"] = torture_catalog
        overall["application_torture_catalog_sha256"] = sha256(torture_catalog_path)
        overall["runtime_fixture"] = {
            "path": str(runtime_fixture), "guest_path_pattern": "/root/.rem-runtime-contract-N.c",
            "sha256": fixture_hash,
        }
        overall["phases"]["ARTIFACT IDENTITY"] = {"status": "PASS", "detail": "Pinned QEMU, kernel and rootfs hashes match."}
        args.results_dir.mkdir(parents=True, exist_ok=True)
        run_dir = Path(tempfile.mkdtemp(prefix="rem-flight-accept-", dir=args.results_dir)).resolve()
        overall["results_dir"] = str(run_dir)
        if args.overlays:
            overlay_meta = json.loads(args.overlays.read_text())
        else:
            overlay_meta = {"artifacts": []}
        checked_overlays = []
        for item in overlay_meta.get("artifacts", []):
            if item.get("accepted") is not True:
                raise AcceptanceError(f"unaccepted artifact in overlay lock: {item.get('name')}")
            source = Path(item["source"])
            if not source.is_absolute():
                source = ROOT / source
            source = source.resolve(strict=True)
            actual = sha256(source)
            if actual != item.get("sha256"):
                raise AcceptanceError(f"overlay SHA-256 mismatch for {item.get('name')}: expected {item.get('sha256')}, got {actual}")
            checked_overlays.append({"name": item.get("name"), "path": str(source),
                                     "sha256": actual, "guest_path": item.get("guest_path")})
        overall["overlays"] = checked_overlays

        merged: dict[str, tuple[str, list[str]]] = {name: ("NOT RUN", []) for name in PHASES}
        severity = {"NOT RUN": -1, "PASS": 0, "NOT INSTALLED": 1,
                    "PENDING OWNER": 2, "TEST ERROR": 3, "FAIL": 4}

        def record(phase: str, status: str, detail: str) -> None:
            prior = merged[phase]
            if severity[status] >= severity[prior[0]]:
                details = prior[1] if status == prior[0] else []
                details.append(detail)
                merged[phase] = (status, details)

        record("ARTIFACT IDENTITY", "PASS", "Pinned QEMU, kernel and rootfs hashes match before execution.")

        final_source_hash = sha256(source_files["rootfs"])
        required_guest_seen = False
        for iteration in range(1, args.repeat + 1):
            directory = run_dir / f"iteration-{iteration}"
            directory.mkdir()
            nonce = secrets.randbelow(800000) + 100000
            runtime_guest_source = f"/root/.rem-runtime-contract-{nonce}.c"
            image = directory / "rootfs.ext4"
            subprocess.run(["cp", "--sparse=always", "--reflink=auto",
                            str(source_files["rootfs"]), str(image)], check=True)
            # The source has already been SHA-256 verified and cp has returned
            # successfully. Rehashing a sparse 10 GiB image reads every hole
            # and adds minutes on each repeat; retain copy size/allocation
            # evidence and recheck the source hash after the suite instead.
            if image.stat().st_size != source_files["rootfs"].stat().st_size:
                raise AcceptanceError("disposable rootfs copy size differs from pinned source")
            initial_copy_hash = "same pinned source via successful cp; full copy hash skipped"
            fixture_stage = run_capture([
                "debugfs", "-w", "-R", f"write {runtime_fixture} {runtime_guest_source}", str(image)
            ])
            if fixture_stage.returncode:
                raise AcceptanceError(f"could not stage runtime fixture in disposable rootfs: {fixture_stage.stderr.strip()}")
            guest_report, guest_errors = verify_guest_image(image, meta)
            fixture_guest_info = check_guest_file(image, runtime_guest_source)
            if not fixture_guest_info or fixture_guest_info.get("sha256") != fixture_hash:
                raise AcceptanceError("staged runtime fixture is missing or its SHA-256 differs from the host fixture")
            guest_report["files"][runtime_guest_source] = fixture_guest_info
            (directory / "guest-artifacts.json").write_text(json.dumps(guest_report, indent=2) + "\n")
            if guest_errors:
                for error in guest_errors:
                    record("ARTIFACT IDENTITY", "FAIL", error)
                raise AcceptanceError("guest rootfs verification failed: " + "; ".join(guest_errors))
            required_guest_seen = True
            meta["verified_guest_files"] = guest_report["files"]
            overlay_error = None
            try:
                staged = apply_overlays(image, args.overlays) if args.overlays else []
            except (AcceptanceError, OSError, subprocess.SubprocessError) as exc:
                staged = []
                overlay_error = str(exc)
                record("CHIBICC", "TEST ERROR", f"accepted overlay setup failed: {exc}")
            available_paths = set(guest_report["files"])
            available_paths.update(item["guest_path"] for item in staged)
            def first_available(paths: list[str]) -> str | None:
                return next((path for path in paths if path in available_paths), None)
            kilo_path = first_available(["/usr/bin/kilo", "/bin/kilo", "/usr/local/bin/kilo"])
            yash_path = first_available(["/usr/bin/yash", "/bin/yash", "/usr/local/bin/yash"])
            chibicc_path = first_available(["/root/native/bin/chibicc", "/usr/bin/chibicc", "/bin/chibicc"])
            samu_lock = meta.get("acceptance_gate", {}).get("tools", {}).get("samu", {})
            candidate_samu_path = first_available(["/root/native/bin/samu", "/usr/bin/samu", "/bin/samu", "/root/samu"])
            samu_path = (candidate_samu_path if samu_lock.get("validated") is True and
                         samu_lock.get("path") == candidate_samu_path else None)
            make_path = first_available(["/usr/bin/make", "/bin/make"])
            installed_vim_path = first_available(["/usr/bin/vim"])
            vim_lock = meta.get("acceptance_gate", {}).get("tools", {}).get("vim", {})
            vim_path = (installed_vim_path if vim_lock.get("validated") is True and
                        vim_lock.get("path") == installed_vim_path else None)
            iteration_report: dict[str, Any] = {
                "iteration": iteration, "seed_rootfs_sha256": final_source_hash,
                "disposable_rootfs_copy_verification": initial_copy_hash,
                "overlays": staged, "guest_files": guest_report,
                "statuses": {}, "logs": [],
            }
            qemu_args = [
                str(source_files["qemu"]), "-M", str(meta.get("machine", "myemulator32")),
                "-m", str(meta.get("memory", "128M")), "-kernel", str(source_files["kernel"]),
                "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk",
                "-nographic", "-monitor", "none", "-serial", "stdio",
                "-icount", str(meta.get("icount", "shift=0,sleep=off")),
            ]
            iteration_report["qemu_argv"] = qemu_args
            def stage(name: str, action):
                guest = None
                log_path = directory / f"{name}.log"
                iteration_report["logs"].append(str(log_path))
                try:
                    guest = boot_guest(qemu_args, log_path, args.timeout)
                    return action(guest)
                except (AcceptanceError, TimeoutError, OSError, subprocess.SubprocessError) as exc:
                    return "FAIL", str(exc)
                finally:
                    if guest is not None:
                        guest.stop()

            boot_log = directory / "boot-1.log"
            iteration_report["logs"].append(str(boot_log))
            guest = None
            try:
                guest = boot_guest(qemu_args, boot_log, args.timeout)
                record("BOOT", "PASS", f"Kernel ran init, ext4 mounted and shell reached a PTY prompt (iteration {iteration}).")
                response = f"REM_HOST_ACK_{nonce}"
                try:
                    console_output = guest.command(
                        f"REM_HOST_ACK_PREFIX=REM_HOST_ACK_; REM_HOST_ACK_SUFFIX={nonce}; "
                        f"echo \"$REM_HOST_ACK_PREFIX$REM_HOST_ACK_SUFFIX\"", f"REM_CONSOLE_{nonce}")
                    if response not in clean_text(console_output):
                        raise AcceptanceError("guest did not return the unique host-command response")
                    record("CONSOLE INPUT", "PASS", f"Guest executed a host command and returned unique response {response}.")
                except (AcceptanceError, TimeoutError) as exc:
                    record("CONSOLE INPUT", "FAIL", str(exc))

                try:
                    shell_value = f"REM_SHELL_ENV_{nonce}"
                    shell_output = guest.command(
                        f"REM_SHELL_PREFIX=REM_SHELL_ENV_; REM_SHELL_SUFFIX={nonce}; "
                        "export REM_SHELL_RESULT=\"$REM_SHELL_PREFIX$REM_SHELL_SUFFIX\"; "
                        "test \"$REM_SHELL_RESULT\" = \"$REM_SHELL_PREFIX$REM_SHELL_SUFFIX\" && "
                        "echo \"$REM_SHELL_RESULT\"",
                        f"REM_SHELL_{nonce}")
                    if shell_value not in clean_text(shell_output):
                        raise AcceptanceError("shell failed command execution or exported-environment expansion")
                    record("SHELL", "PASS", f"Interactive init shell executed a command and expanded exported environment value {shell_value}.")
                except (AcceptanceError, TimeoutError) as exc:
                    record("SHELL", "FAIL", str(exc))

                fs_value = f"REM_FILE_CONTENT_{nonce}"
                guest_file = f"/root/.rem-accept-{nonce}.txt"
                lifecycle_file = f"/root/.rem-fs-lifecycle-{nonce}.txt"
                try:
                    fs_output = guest.command(
                        f"REM_FILE_PREFIX=REM_FILE_CONTENT_; REM_FILE_SUFFIX={nonce}; "
                        f"echo \"$REM_FILE_PREFIX$REM_FILE_SUFFIX\" > {guest_file} && "
                        f"echo OLD_{nonce} > {lifecycle_file} && "
                        f"echo NEW_{nonce} > {lifecycle_file} && "
                        f"test \"$(cat {lifecycle_file})\" = NEW_{nonce} && "
                        f"cat {guest_file} && rm {lifecycle_file} && test ! -e {lifecycle_file}",
                        f"REM_FS_{nonce}")
                    if fs_value not in clean_text(fs_output):
                        raise AcceptanceError("guest filesystem read-back did not match the generated value")
                    record("FILESYSTEM", "PASS", "Created/read marker, modified/read a second file, deleted it, and verified it was absent.")
                except (AcceptanceError, TimeoutError) as exc:
                    record("FILESYSTEM", "FAIL", str(exc))
            except (AcceptanceError, TimeoutError, OSError, subprocess.SubprocessError) as exc:
                record("BOOT", "FAIL", str(exc))
                for phase in ("CONSOLE INPUT", "SHELL", "FILESYSTEM", "YASH", "KILO", "VIM", "CHIBICC", "RUNTIME ABI", "MAKE", "BINUTILS ARCHIVE", "SAMURAI", "APPLICATION TORTURE", "NETWORK", "PERSISTENCE"):
                    record(phase, "TEST ERROR", "Guest did not reach the interactive shell.")
                iteration_report["error"] = str(exc)
            finally:
                if guest is not None:
                    guest.stop()

            if "error" not in iteration_report:
                if yash_path:
                    status, detail = stage("yash", lambda g: run_yash(g, yash_path, nonce))
                    record("YASH", status, detail)
                else:
                    record("YASH", "PENDING OWNER", "No accepted Yash binary is integrated in the selected flight rootfs.")

                if kilo_path:
                    status, detail = stage("kilo", lambda g: run_kilo(g, kilo_path, nonce))
                    record("KILO", status, detail)
                else:
                    record("KILO", "NOT INSTALLED", "No Kilo executable is installed in the selected flight rootfs.")

                if installed_vim_path and not vim_path:
                    record("VIM", "PENDING OWNER", "Vim is installed but lacks a validated exact-path/hash entry in acceptance_gate.tools.")
                elif not vim_path:
                    record("VIM", "NOT INSTALLED", "No Vim executable is installed in the selected flight rootfs.")
                else:
                    status, detail = stage("vim", lambda g: run_vim(g, vim_path, nonce))
                    record("VIM", status, detail)

                if overlay_error:
                    pass
                elif chibicc_path:
                    def chibicc_test(g: Guest) -> tuple[str, str]:
                        try:
                            g.command(
                                f"chmod 755 {chibicc_path}; echo 'int puts(const char *);' > /root/accept-hello.c; "
                                f"echo 'int main(void) {{ puts(\"REM_NATIVE_\" \"HELLO_\" \"ACCEPT\"); return 0; }}' "
                                f">> /root/accept-hello.c",
                                f"REM_CHIBICC_PREP_{nonce}")
                            g.command(f"{chibicc_path} -S -o /root/accept-hello.s /root/accept-hello.c && test -s /root/accept-hello.s",
                                      f"REM_CHIBICC_COMPILE_{nonce}", timeout=20)
                            g.command("/root/native/bin/as -o /root/accept-hello.o /root/accept-hello.s && test -s /root/accept-hello.o",
                                      f"REM_ASSEMBLE_{nonce}", timeout=20)
                            g.command(
                                "/root/native/bin/ld -T /root/native/lib/myemulator2-user.ld "
                                "-o /root/accept-hello /usr/lib/crt1.o /usr/lib/crti.o "
                                "/root/accept-hello.o /usr/lib/libc.a /root/native/lib/libgcc.a /usr/lib/crtn.o",
                                f"REM_LINK_{nonce}", timeout=20)
                            output = g.command("/root/accept-hello", f"REM_CHIBICC_RUN_{nonce}")
                            if "REM_NATIVE_HELLO_ACCEPT" not in clean_text(output):
                                raise AcceptanceError("native chibicc executable output did not match exactly")
                            g.command(
                                f"echo 'int puts(const char *);' > /root/accept-hello.c; "
                                f"echo 'int main(void) {{ puts(\"REM_NATIVE_EDITED_ACCEPT_{nonce}\"); return 0; }}' >> /root/accept-hello.c; "
                                f"{chibicc_path} -S -o /root/accept-hello.s /root/accept-hello.c && "
                                "/root/native/bin/as -o /root/accept-hello.o /root/accept-hello.s && "
                                "/root/native/bin/ld -T /root/native/lib/myemulator2-user.ld "
                                "-o /root/accept-hello /usr/lib/crt1.o /usr/lib/crti.o "
                                "/root/accept-hello.o /usr/lib/libc.a /root/native/lib/libgcc.a /usr/lib/crtn.o",
                                f"REM_DEV_EDIT_BUILD_{nonce}", timeout=30)
                            edited = g.command("/root/accept-hello", f"REM_DEV_EDIT_RUN_{nonce}")
                            if f"REM_NATIVE_EDITED_ACCEPT_{nonce}" not in clean_text(edited):
                                raise AcceptanceError("edited source rebuild did not print its changed output")
                            return "PASS", "Created C source, edited its contents, compiled, assembled and linked with pinned tools, and verified both executable outputs."
                        except (AcceptanceError, TimeoutError) as exc:
                            return "FAIL", str(exc)
                    status, detail = stage("chibicc", chibicc_test)
                    record("CHIBICC", status, detail)
                else:
                    record("CHIBICC", "PENDING OWNER", "No accepted compiler overlay or installed chibicc found.")

                if chibicc_path and not overlay_error:
                    status, detail = stage("runtime-abi", lambda g: run_runtime_contract(g, nonce, runtime_guest_source))
                    record("RUNTIME ABI", status, detail)
                else:
                    record("RUNTIME ABI", "PENDING OWNER", "Runtime ABI fixtures require the validated native chibicc/as/ld toolchain.")

                locked_make = meta.get("acceptance_gate", {}).get("tools", {}).get("make", {})
                if not make_path:
                    record("MAKE", "NOT INSTALLED", "No make executable is installed in this candidate.")
                elif (locked_make.get("validated") is not True or
                      locked_make.get("path") != make_path):
                    record("MAKE", "PENDING OWNER", "make is installed but has no validated exact-path/hash entry in acceptance_gate.tools.")
                elif not chibicc_path or overlay_error:
                    record("MAKE", "PENDING OWNER", "make workflow requires the candidate's validated chibicc toolchain.")
                else:
                    status, detail = stage("make", lambda g: run_make(g, nonce))
                    record("MAKE", status, detail)

                if chibicc_path and not overlay_error:
                    status, detail = stage("ar-ranlib", lambda g: run_ar_ranlib(g, nonce))
                    record("BINUTILS ARCHIVE", status, detail)
                else:
                    record("BINUTILS ARCHIVE", "PENDING OWNER", "Archive acceptance requires the unified candidate's validated chibicc/as/ld/ar/ranlib tools.")

                if samu_path:
                    status, detail = stage("samurai", lambda g: run_samurai(g, samu_path, nonce, bool(chibicc_path) and not overlay_error))
                    record("SAMURAI", status, detail)
                else:
                    record("SAMURAI", "PENDING OWNER", "No accepted Samurai executable is integrated in the selected flight rootfs.")

                # Application torture is deliberately after Samurai, so any
                # workload regression is reported without suppressing its run.
                torture_results = []
                for definition in torture_catalog.get("tests", []):
                    test_dir = directory / "torture"
                    test_dir.mkdir(exist_ok=True)
                    log_path = test_dir / f"{definition['id']}.log"
                    guest_case = None
                    try:
                        guest_case = boot_guest(qemu_args, log_path, args.timeout)
                        result = run_torture_case(guest_case, definition, nonce, meta)
                    except (AcceptanceError, TimeoutError, OSError, subprocess.SubprocessError) as exc:
                        guest_artifacts_by_path = {item.get("path"): item for item in meta.get("guest_artifacts", [])}
                        verified_guest_files = meta.get("verified_guest_files", {})
                        candidate_binaries = definition.get("binary_candidates", [])
                        result = {
                            "id": definition.get("id"), "tier": definition.get("tier"),
                            "subsystem": definition.get("subsystem"),
                            "status": "FAIL", "detail": str(exc),
                            "binary_hashes": {
                                path: (verified_guest_files.get(path, {}).get("sha256") or
                                       guest_artifacts_by_path.get(path, {}).get("sha256"))
                                for path in candidate_binaries
                            },
                            "binary_hash_sources": {
                                path: ("offline guest image verification" if verified_guest_files.get(path, {}).get("sha256")
                                       else "candidate manifest pin" if guest_artifacts_by_path.get(path, {}).get("sha256")
                                       else "unavailable")
                                for path in candidate_binaries
                            },
                            "compiler_used": {path: compiler_provenance(meta, path)
                                              for path in candidate_binaries},
                            "command": definition.get("command", "").format(
                                n=nonce, bin=candidate_binaries[0] if candidate_binaries else ""),
                            "expected_result": definition.get("expected_result", definition.get("expected")),
                            "failure_layer": definition.get("subsystem"),
                            "fault_pc": fault_pc_from_log(log_path),
                            "minimal_reproducer": definition.get("minimal_reproducer", "").format(
                                n=nonce, bin=candidate_binaries[0] if candidate_binaries else ""),
                            "serial_log": str(log_path),
                        }
                        if log_path.exists():
                            result["serial_tail"] = log_path.read_text(errors="replace")[-6000:]
                    finally:
                        if guest_case is not None:
                            guest_case.stop()
                    torture_results.append(result)
                    record("APPLICATION TORTURE", result["status"], f"{result['id']}: {result.get('detail', '')}")
                iteration_report["application_torture"] = torture_results
                if meta.get("network", {}).get("accepted"):
                    status, detail = stage("network", lambda g: run_network(g, meta, nonce))
                    record("NETWORK", status, detail)
                else:
                    record("NETWORK", "PENDING OWNER", "No accepted network-enabled flight tuple/endpoint is declared (network owner #1).")

                # Persistence is last so it covers the final development
                # state produced by the application and build checks above.
                sync_guest = None
                guest2 = None
                try:
                    sync_guest = boot_guest(qemu_args, directory / "sync-final.log", args.timeout)
                    iteration_report["logs"].append(str(directory / "sync-final.log"))
                    persistent_paths = [guest_file]
                    persistent_paths.append(runtime_guest_source)
                    if kilo_path and merged["KILO"][0] == "PASS":
                        persistent_paths.append(f"/root/rem-kilo-accept-{nonce}.txt")
                    if vim_path and merged["VIM"][0] == "PASS":
                        persistent_paths.append(f"/root/rem-vim-accept-{nonce}.txt")
                    if chibicc_path and merged["CHIBICC"][0] == "PASS":
                        persistent_paths.extend(("/root/accept-hello.c", "/root/accept-hello.s", "/root/accept-hello.o", "/root/accept-hello"))
                    if chibicc_path and merged["RUNTIME ABI"][0] == "PASS":
                        persistent_paths.extend((f"/root/rem-runtime-contract-{nonce}.s",
                            f"/root/rem-runtime-contract-{nonce}.o",
                            f"/root/rem-runtime-contract-{nonce}"))
                    if make_path and merged["MAKE"][0] == "PASS":
                        persistent_paths.extend((f"/root/rem-make-{nonce}/hello.c", f"/root/rem-make-{nonce}/hello"))
                    if samu_path and merged["SAMURAI"][0] == "PASS":
                        persistent_paths.extend((f"/root/samu-accept-{nonce}.c", f"/root/samu-accept-{nonce}.ninja", f"/root/samu-accept-{nonce}"))
                    if merged["BINUTILS ARCHIVE"][0] == "PASS":
                        persistent_paths.extend((f"/root/rem-archive-{nonce}/member.c", f"/root/rem-archive-{nonce}/main.c", f"/root/rem-archive-{nonce}/librem.a", f"/root/rem-archive-{nonce}/archive-test"))
                    paths_arg = " ".join(persistent_paths)
                    before_hashes = parse_guest_hashes(
                        sync_guest.command(f"sha256sum {paths_arg}", f"REM_HASH_BEFORE_{nonce}"))
                    if set(before_hashes) != set(persistent_paths):
                        raise AcceptanceError("pre-reboot SHA-256 output omitted one or more test files")
                    iteration_report["persistent_file_hashes_before_reboot"] = before_hashes
                    sync_guest.command("sync", f"REM_SYNC_FINAL_{nonce}")
                    sync_guest.stop()
                    sync_guest = None

                    reboot_log = directory / "boot-final.log"
                    iteration_report["logs"].append(str(reboot_log))
                    guest2 = boot_guest(qemu_args, reboot_log, args.timeout)
                    persisted = guest2.command(f"cat {guest_file}", f"REM_PERSIST_{nonce}")
                    if fs_value not in clean_text(persisted):
                        raise AcceptanceError("filesystem marker did not survive final synced reboot")
                    guest2.command(f"test ! -e {lifecycle_file}", f"REM_FS_DELETE_PERSIST_{nonce}")
                    after_hashes = parse_guest_hashes(
                        guest2.command(f"sha256sum {paths_arg}", f"REM_HASH_AFTER_{nonce}"))
                    iteration_report["persistent_file_hashes_after_reboot"] = after_hashes
                    if after_hashes != before_hashes:
                        raise AcceptanceError(f"persistent file hashes changed over reboot: before={before_hashes}; after={after_hashes}")
                    record("PERSISTENCE", "PASS", f"Final sync/reboot preserved exact SHA-256 hashes for {len(persistent_paths)} filesystem/workspace files.")
                except (AcceptanceError, TimeoutError, OSError, subprocess.SubprocessError) as exc:
                    record("PERSISTENCE", "FAIL", str(exc))
                finally:
                    if sync_guest is not None:
                        sync_guest.stop()
                    if guest2 is not None:
                        guest2.stop()
            # The disposable image is intentionally modified by guest writes;
            # hashing its full 10 GiB virtual length here is expensive and does
            # not add identity evidence. The pinned source hash is checked
            # again after all iterations below.
            final_fsck = run_capture(["e2fsck", "-fn", str(image)], timeout=180)
            iteration_report["final_e2fsck_returncode"] = final_fsck.returncode
            iteration_report["final_e2fsck_output"] = (final_fsck.stdout + final_fsck.stderr)[-4000:]
            if final_fsck.returncode != 0:
                record("PERSISTENCE", "FAIL", f"post-test e2fsck -fn returned {final_fsck.returncode}")
            iteration_report["disposable_rootfs_size_bytes"] = image.stat().st_size
            iteration_report["disposable_rootfs_allocated_bytes"] = image.stat().st_blocks * 512
            iteration_report["statuses"] = {
                name: {"status": value[0], "detail": "; ".join(value[1])}
                for name, value in merged.items()
            }
            phase_statuses = [item["status"] for item in iteration_report["statuses"].values()]
            iteration_report["status"] = (
                "FAIL" if iteration_report.get("error") or any(
                    status in ("FAIL", "TEST ERROR") for status in phase_statuses
                ) else "PASS"
            )
            overall["iterations"].append(iteration_report)

        for phase, value in list(merged.items()):
            if value[0] == "NOT RUN":
                record(phase, "TEST ERROR", "not reached because an earlier acceptance stage stopped this run")
        source_hash_after = sha256(source_files["rootfs"])
        overall["source_rootfs_sha256_after"] = source_hash_after
        if source_hash_after != final_source_hash:
            record("ARTIFACT IDENTITY", "FAIL", "Pinned source rootfs changed during acceptance run.")
        overall["phases"] = {name: {"status": value[0], "detail": "; ".join(value[1])}
                             for name, value in merged.items()}
        if not required_guest_seen:
            overall["phases"]["ARTIFACT IDENTITY"] = {"status": "FAIL", "detail": "required guest artifacts were not verified"}
        statuses = [item["status"] for item in overall["phases"].values()]
        if any(status in ("FAIL", "TEST ERROR") for status in statuses):
            overall["status"] = "FAIL"
        elif all(status == "PASS" for status in statuses):
            overall["status"] = "PASS"
        else:
            overall["status"] = "PARTIAL"
        overall["finished_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        (run_dir / "result.json").write_text(json.dumps(overall, indent=2) + "\n")
    except (AcceptanceError, OSError, ValueError, KeyError, json.JSONDecodeError,
            subprocess.SubprocessError) as exc:
        overall["status"] = "TEST ERROR"
        overall["error"] = str(exc)
        if overall["phases"]["ARTIFACT IDENTITY"]["status"] == "TEST ERROR":
            overall["phases"]["ARTIFACT IDENTITY"] = {"status": "FAIL", "detail": str(exc)}

    print("REM FLIGHT ACCEPTANCE")
    if overall.get("tuple"):
        print(f"Tuple: {overall['tuple'].get('name', overall['tuple'].get('tuple', 'unnamed'))}")
    for key in ("qemu", "kernel", "rootfs"):
        if key in overall.get("artifacts", {}):
            item = overall["artifacts"][key]
            print(f"{key.upper():7} {item['sha256']}  {item['path']}")
    for phase in PHASES:
        item = overall["phases"][phase]
        print(f"{phase:17} {item['status']}")
        if item.get("detail") and item["status"] not in ("PASS", "PENDING OWNER", "NOT INSTALLED"):
            print(f"  {item['detail']}")
    print(f"OVERALL           {overall['status']}")
    if overall.get("results_dir"):
        print(f"REPORT            {overall['results_dir']}/result.json")
    if overall.get("error"):
        print(f"ERROR             {overall['error']}")
    return 0 if overall["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
