#!/usr/bin/env python3
"""Run pinned OpenSSH build outputs on an isolated copy of a REM flight tuple."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[2]
ACCEPTANCE_PATH = ROOT / "tools/rem-flight-acceptance.py"
DEFAULT_TUPLE = ROOT / "tools/rem-flight-image/tuples/flight-20260927-chibicc-level5.json"
BINARIES = {
    "ssh": ("ssh", "/usr/bin/ssh", 0o100755),
    "sshd": ("sshd", "/usr/sbin/sshd", 0o100755),
    "ssh-keygen": ("ssh-keygen", "/usr/bin/ssh-keygen", 0o100755),
    "sshd-auth": ("sshd-auth", "/usr/libexec/sshd-auth", 0o100755),
    "sshd-session": ("sshd-session", "/usr/libexec/sshd-session", 0o100755),
    "sftp-server": ("sftp-server", "/usr/libexec/sftp-server", 0o100755),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_acceptance():
    spec = importlib.util.spec_from_file_location("rem_acceptance", ACCEPTANCE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load REM acceptance helpers: {ACCEPTANCE_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def debugfs(image: Path, command: str, writable: bool = False) -> str:
    argv = ["debugfs"]
    if writable:
        argv.append("-w")
    argv += ["-R", command, str(image)]
    result = subprocess.run(argv, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"debugfs {command!r} failed: {result.stderr} {result.stdout}")
    return result.stdout + result.stderr


def write_guest(image: Path, host: Path, guest: str, mode: int | None = None) -> None:
    debugfs(image, f"write {host} {guest}", writable=True)
    if mode is not None:
        debugfs(image, f"set_inode_field {guest} mode {mode:o}", writable=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tuple", type=Path, default=DEFAULT_TUPLE)
    parser.add_argument("--candidate-dir", type=Path,
                        help="use an explicitly hashed candidate manifest and output rootfs")
    parser.add_argument("--refresh-candidate-rootfs-hash", action="store_true",
                        help="accept a changed candidate output hash only after verifying all manifest guest files")
    parser.add_argument("--build-dir", type=Path, required=True,
                        help="directory containing OpenSSH executables from the controlled build")
    parser.add_argument("--runtime-probe", type=Path,
                        help="optional statically linked REM ELF testing sockets/processes/signals/modes/PTYs")
    parser.add_argument("--report", type=Path,
                        help="JSON report destination (default: alongside --build-dir)")
    parser.add_argument("--boot-timeout", type=int, default=120)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve(strict=True)
    if args.candidate_dir:
        candidate_dir = args.candidate_dir.resolve(strict=True)
        tuple_path = candidate_dir / "manifest.json"
        tuple_data = json.loads(tuple_path.read_text())
        rootfs_item = tuple_data.get("outputs", {}).get("rootfs")
        if not rootfs_item or not re.fullmatch(r"[0-9a-f]{64}", rootfs_item.get("sha256", "")):
            raise RuntimeError("candidate manifest does not pin outputs.rootfs SHA-256")
        files = {
            "qemu": None, "kernel": None,
            "rootfs": (candidate_dir / "rem-rootfs.ext4").resolve(strict=True),
        }
        expected_hashes = {
            key: tuple_data["inputs"][key]["sha256"] for key in ("qemu", "kernel")
        }
        expected_hashes["rootfs"] = rootfs_item["sha256"]
    else:
        tuple_path = args.tuple.resolve(strict=True)
        tuple_data = json.loads(tuple_path.read_text())
        files = {}
        expected_hashes = {}
        for key in ("qemu", "kernel", "rootfs"):
            item = tuple_data["inputs"][key]
            path = Path(item["path"])
            if not path.is_absolute():
                path = ROOT / path
            files[key] = path.resolve(strict=True)
            expected_hashes[key] = item["sha256"]
    identities = {}
    for name in ("qemu", "kernel", "rootfs"):
        path = files[name]
        if path is None:
            item = tuple_data["inputs"][name]
            path = Path(item["path"])
            if not path.is_absolute():
                path = ROOT / path
            path = path.resolve(strict=True)
            files[name] = path
        actual = sha256(path)
        manifest_hash_drift = None
        if actual != expected_hashes[name]:
            if not (args.candidate_dir and name == "rootfs" and args.refresh_candidate_rootfs_hash):
                raise RuntimeError(f"pinned {name} hash mismatch: expected {expected_hashes[name]}, found {actual}")
            manifest_hash_drift = expected_hashes[name]
            expected_hashes[name] = actual
        identities[name] = {"path": str(path), "sha256": actual}
        if manifest_hash_drift:
            identities[name]["candidate_manifest_sha256"] = manifest_hash_drift
            identities[name]["manifest_hash_drift"] = True

    binary_identity = {}
    for name, (filename, _guest, _mode) in BINARIES.items():
        path = build_dir / filename
        if not path.is_file():
            raise RuntimeError(f"missing expected OpenSSH output: {path}")
        header = subprocess.run(["readelf", "-h", str(path)], text=True, capture_output=True)
        if header.returncode:
            raise RuntimeError(f"readelf failed for {name}: {header.stderr.strip()}")
        machine_match = re.search(r"^\s*Machine:\s*(.*?)\s*$", header.stdout, re.M)
        machine = machine_match.group(1) if machine_match else "unknown"
        if machine != "<unknown>: 0xf2e2":
            raise RuntimeError(f"{name} has unexpected ELF machine: {machine}")
        dynamic = subprocess.run(["readelf", "-d", str(path)], text=True, capture_output=True)
        has_dynamic_section = "There is no dynamic section" not in dynamic.stdout
        if dynamic.returncode or has_dynamic_section:
            raise RuntimeError(f"{name} is not a valid static REM ELF: {dynamic.stderr.strip() or dynamic.stdout.strip()}")
        binary_identity[name] = {
            "path": str(path), "sha256": sha256(path), "size": path.stat().st_size,
            "elf_machine": machine, "has_dynamic_section": has_dynamic_section,
        }

    probe_path = args.runtime_probe.resolve(strict=True) if args.runtime_probe else None
    probe_identity = None
    if probe_path:
        header = subprocess.run(["readelf", "-h", str(probe_path)], text=True, capture_output=True)
        machine_match = re.search(r"^\s*Machine:\s*(.*?)\s*$", header.stdout, re.M)
        dynamic = subprocess.run(["readelf", "-d", str(probe_path)], text=True, capture_output=True)
        if header.returncode or not machine_match or machine_match.group(1) != "<unknown>: 0xf2e2":
            raise RuntimeError(f"runtime probe is not a REM ELF: {probe_path}")
        if dynamic.returncode or "There is no dynamic section" not in dynamic.stdout:
            raise RuntimeError(f"runtime probe must be statically linked: {probe_path}")
        probe_identity = {"path": str(probe_path), "sha256": sha256(probe_path),
                          "size": probe_path.stat().st_size,
                          "elf_machine": machine_match.group(1), "static": True}

    result: dict = {
        "tuple": tuple_data.get("name", tuple_data.get("tuple", "unnamed")),
        "tuple_artifacts": identities, "runtime_probe_artifact": probe_identity,
        "openssh_artifacts": binary_identity, "status": "FAIL", "tests": {},
    }
    report_path = args.report or (build_dir.parent / "guest-report.json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    accept = load_acceptance()
    # Flight init scripts use REM# while the small regression images use #.
    accept.PROMPT = re.compile(rb"(?:^|\r*\n|\x1b\[\?1049l)(?:REM)?# ")

    with tempfile.TemporaryDirectory(prefix="rem-openssh-accept-") as temp_name:
        temp = Path(temp_name)
        image = temp / "rootfs.ext4"
        subprocess.run(["cp", "--sparse=always", "--reflink=auto", str(files["rootfs"]), str(image)], check=True)
        if image.stat().st_size != files["rootfs"].stat().st_size:
            raise RuntimeError("disposable rootfs copy size differs from pinned source")
        if args.candidate_dir:
            _guest_report, guest_errors = accept.verify_guest_image(image, tuple_data)
            if guest_errors:
                raise RuntimeError("candidate guest artifact verification failed: " + "; ".join(guest_errors))
        for name, (filename, guest, mode) in BINARIES.items():
            write_guest(image, build_dir / filename, guest, mode)
        if probe_path:
            write_guest(image, probe_path, "/usr/bin/rem-openssh-runtime-probe", 0o100755)

        # The seed flight rootfs intentionally has no passwd/group database.
        # These minimal disposable records provide normal root and privsep
        # account lookup so sshd tests its real account-resolution path.
        passwd = temp / "passwd"
        passwd.write_text("root:x:0:0:root:/root:/bin/ash\nsshd:x:74:74:sshd privilege separation:/var/empty:/sbin/nologin\n")
        group = temp / "group"
        group.write_text("root:x:0:\nsshd:x:74:\n")
        write_guest(image, passwd, "/etc/passwd", 0o100644)
        write_guest(image, group, "/etc/group", 0o100644)
        if "Type: directory" not in debugfs(image, "stat /var/empty"):
            debugfs(image, "mkdir /var/empty", writable=True)
        debugfs(image, "set_inode_field /var/empty mode 40755", writable=True)

        config = temp / "sshd_config"
        config.write_text(
            "Port 22331\nListenAddress 127.0.0.1\nAddressFamily inet\n"
            "HostKey /tmp/rem-ssh-host-key\nPidFile /tmp/rem-ssh.pid\n"
            "AuthorizedKeysFile /tmp/rem-ssh-authorized_keys\n"
            "PermitRootLogin yes\nPubkeyAuthentication yes\nPasswordAuthentication no\n"
            "StrictModes no\nAllowUsers root\n"
        )
        write_guest(image, config, "/tmp/rem-sshd_config", 0o100644)
        qemu_args = [
            str(files["qemu"]), "-M", str(tuple_data.get("machine", "myemulator32")),
            "-m", str(tuple_data.get("memory", "128M")), "-kernel", str(files["kernel"]),
            "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk",
            "-nographic", "-monitor", "none", "-serial", "stdio",
            "-icount", str(tuple_data.get("icount", "shift=0,sleep=off")),
        ]
        if tuple_data.get("append"):
            qemu_args += ["-append", str(tuple_data["append"])]
        if tuple_data.get("network_user") is True:
            # The REM machine creates its own virtio-mmio NIC when net0 exists.
            qemu_args += ["-netdev", "user,id=net0"]
        guest = None
        try:
            # Record a guest user fault as the outcome of its individual
            # command, then keep the shell alive for later independent probes.
            accept.FATAL = re.compile(
                rb"Kernel panic|Attempted to kill init|Failed to execute /sbin/init|"
                rb"EXT4-fs error|QEMU:.*(error|failed)|qemu-system-myemulator32:.*(error|failed)", re.I
            )
            # Keep the process handle even if boot fails so finally can preserve
            # the partial serial transcript and QEMU diagnostics.
            guest = accept.Guest(qemu_args, temp / "serial.log", args.boot_timeout)
            guest.expect(re.compile(rb"VFS: Mounted root \(ext4 filesystem\)"), args.boot_timeout)
            guest.expect(re.compile(rb"Run /sbin/init as init process"), args.boot_timeout)
            guest.expect(accept.PROMPT, args.boot_timeout)
            guest.command("stty -echo", "REM_ACCEPT_DISABLE_ECHO", min(args.boot_timeout, 8))

            def run_case(name: str, command: str, marker: str, expected: str | None = None,
                         timeout: int = 30) -> str:
                try:
                    guest.sendline(f"{command}; rc=$?; echo {marker}:$rc")
                    match_re = re.compile(rb"\r*\n" + re.escape(marker.encode()) + rb":([0-9]+)\r*\n")
                    output = guest.expect(match_re, timeout)
                    match = match_re.search(output)
                    status = match.group(1).decode() if match else "unknown"
                    guest.expect(accept.PROMPT, timeout)
                    clean = accept.clean_text(output.decode("latin1", "replace"))
                    if status != "0":
                        result["tests"][name] = {
                            "status": "FAIL", "returncode": status, "output": clean[-3000:],
                        }
                        return clean
                    if expected and expected not in clean:
                        result["tests"][name] = {
                            "status": "FAIL", "returncode": status,
                            "error": f"expected output {expected!r} not observed", "output": clean[-3000:],
                        }
                        return clean
                    result["tests"][name] = {"status": "PASS", "returncode": status, "output": clean[-3000:]}
                    return clean
                except Exception as exc:
                    result["tests"][name] = {"status": "FAIL", "error": str(exc)}
                    return ""

            if probe_path:
                probe_output = run_case("dependency probes", "/usr/bin/rem-openssh-runtime-probe",
                                        "REM_SSH_RUNTIME_PROBE")
                for match in re.finditer(r"^([A-Z0-9_]+)=(PASS|FAIL) errno=([0-9]+)$", probe_output, re.M):
                    name, status, error = match.groups()
                    result["tests"][f"probe: {name}"] = {
                        "status": status, "errno": int(error), "output": match.group(0),
                    }

            version = run_case("ssh -V", "/usr/bin/ssh -V 2>&1", "REM_SSH_VERSION", "OpenSSH_10.5")
            run_case("ssh-keygen RSA host key", "/usr/bin/ssh-keygen -q -t rsa -b 2048 -N '' -f /tmp/rem-ssh-host-key", "REM_SSH_HOST_KEY")
            run_case("ssh-keygen RSA client key", "/usr/bin/ssh-keygen -q -t rsa -b 2048 -N '' -f /tmp/rem-ssh-client-key", "REM_SSH_CLIENT_KEY")
            run_case("private-key mode", "test \"$(stat -c %a /tmp/rem-ssh-host-key)\" = 600", "REM_SSH_KEY_MODE")
            run_case("sshd -t", "/usr/sbin/sshd -t -f /tmp/rem-sshd_config", "REM_SSHD_CONFIG")
            run_case("authorized key staging", "cat /tmp/rem-ssh-client-key.pub > /tmp/rem-ssh-authorized_keys && chmod 600 /tmp/rem-ssh-authorized_keys", "REM_SSH_AUTHORIZED_KEY")
            run_case("sshd daemon startup", "/usr/sbin/sshd -f /tmp/rem-sshd_config", "REM_SSHD_START", timeout=40)
            time.sleep(1)
            remote = run_case(
                "localhost authenticated command",
                "/usr/bin/ssh -p 22331 -i /tmp/rem-ssh-client-key -o IdentitiesOnly=yes "
                "-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/tmp/rem-ssh_known_hosts "
                "root@127.0.0.1 'echo REM_SSH_LOCAL_AUTH_OK'",
                "REM_SSH_LOCAL_AUTH", "REM_SSH_LOCAL_AUTH_OK", 60,
            )
            run_case(
                "localhost PTY allocation",
                "/usr/bin/ssh -tt -p 22331 -i /tmp/rem-ssh-client-key -o IdentitiesOnly=yes "
                "-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/tmp/rem-ssh_known_hosts "
                "root@127.0.0.1 'test -t 0 && echo REM_SSH_PTY_OK'",
                "REM_SSH_PTY", "REM_SSH_PTY_OK", 60,
            )
            run_case("sshd signal shutdown", "kill -TERM \"$(cat /tmp/rem-ssh.pid)\"", "REM_SSHD_STOP", timeout=20)
            statuses = [entry["status"] for entry in result["tests"].values()]
            result["status"] = "PASS" if statuses and all(s == "PASS" for s in statuses) else "PARTIAL"
            if version and "OpenSSH_10.5" not in version:
                result["status"] = "FAIL"
        except Exception as exc:
            result["boot_error"] = str(exc)
        finally:
            if guest is not None:
                result["serial_log"] = str(report_path.parent / "serial.log")
                guest.stop()
                shutil.copy2(temp / "serial.log", report_path.parent / "serial.log")

    result["source_rootfs_sha256_after"] = sha256(files["rootfs"])
    result["source_rootfs_unchanged"] = result["source_rootfs_sha256_after"] == identities["rootfs"]["sha256"]
    report_path.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "PASS" and result["source_rootfs_unchanged"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
