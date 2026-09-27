#!/usr/bin/env python3
"""Qualify guest-native REM binutils on a disposable flight rootfs copy.

Requires QEMU, debugfs, pexpect, the validated flight tuple, and the staged
native tools in toolchain/flight-kit-runtime. Never writes the source image.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

import pexpect

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / ".rem-phone-deploy-20260925/REM-FLIGHT"
STAGING = ROOT / "toolchain/flight-kit-runtime"
FIXTURES = ROOT / "toolchain/tests/native-binutils"
TEST_TOOLS = ("ar", "as", "ld", "ranlib", "readelf", "objdump",
              "objcopy", "nm", "strip", "chibicc")
EXPECTED_CORE = {
    "as": "c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93",
    "ld": "edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65",
    "ar": "31585d2cfa5dcdf702e4655b2ac386b7580641aaa378d2b39bde106bb4543ba7",
    "ranlib": "b2f8ac9eba8000c34c1f609d1955bc8e3495532b3bf0739ad714554e36d00bd3",
    "objdump": "b7547055d61a954aeaaeae41240f8ef1aee0ac0456cfa43b1cbe03958091fbdc",
    "readelf": "778d9e788068b2d8b78e85bfa2cf99c1c51f78873c20939a2a8bd91eb01b2eaa",
    "chibicc": "7b9134caaf776953d82d34c3f54dec0eb5b3f343d8ec92ff350df6ee82023a75",
}
PINNED_ROOTFS_SHA256 = "46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd"
GUEST_TOOL_DIR = "/root/binqual/bin"
GUEST_LIB_DIR = "/root/binqual/lib"
GUEST_SRC_DIR = "/root/binqual/src"
GUEST_WORK_DIR = "/root/binqual/work"
PROMPT = r"(?:\r?\n|\x1b\[\?1049l)# "


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run_debugfs(image: Path, commands: list[str], command_file: Path) -> str:
    command_file.write_text("\n".join(commands) + "\n", encoding="utf-8")
    proc = subprocess.run(["debugfs", "-w", "-f", str(command_file), str(image)],
                          text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, check=False)
    if proc.returncode:
        raise RuntimeError(f"debugfs failed ({proc.returncode}):\n{proc.stdout}")
    return proc.stdout


def prepare_host_archive(out: Path) -> Path:
    """Make an input archive only to isolate guest ranlib/readelf behavior."""
    host_bin = ROOT / ".toolchain-install/bin"
    assembler = host_bin / "myemulator2-elf-as"
    archiver = host_bin / "myemulator2-elf-ar"
    fixture_dir = out / "host-archive-input"
    fixture_dir.mkdir()
    shutil.copy2(FIXTURES / "archive-a.S", fixture_dir / "archive-a.S")
    subprocess.run([str(assembler), "-o", "archive-a.o", "archive-a.S"],
                   cwd=fixture_dir, check=True)
    subprocess.run([str(archiver), "rcs", "host-index-input.a", "archive-a.o"],
                   cwd=fixture_dir, check=True)
    return fixture_dir / "host-index-input.a"


def inject(image: Path, out: Path, command_file: Path,
           host_archive: Path) -> dict[str, str]:
    commands = [
        f"mkdir {GUEST_TOOL_DIR.rsplit('/', 1)[0]}",
        f"mkdir {GUEST_TOOL_DIR}",
        f"mkdir {GUEST_TOOL_DIR}/include",
        f"mkdir {GUEST_LIB_DIR}",
        f"mkdir {GUEST_SRC_DIR}",
        f"mkdir {GUEST_WORK_DIR}",
        f"mkdir {GUEST_WORK_DIR}/extracted",
    ]
    hashes: dict[str, str] = {}
    for tool in TEST_TOOLS:
        src = STAGING / "bin" / tool
        if not src.is_file():
            raise FileNotFoundError(src)
        dest = f"{GUEST_TOOL_DIR}/{tool}"
        commands.append(f"write {src} {dest}")
        hashes[f"bin/{tool}"] = sha256(src)
    for header in sorted((STAGING / "bin/include").glob("*.h")):
        dest = f"{GUEST_TOOL_DIR}/include/{header.name}"
        commands.append(f"write {header} {dest}")
        hashes[f"bin/include/{header.name}"] = sha256(header)

    for lib in ("libgcc.a",):
        src = STAGING / "lib" / lib
        commands.append(f"write {src} {GUEST_LIB_DIR}/{lib}")
        hashes[f"lib/{lib}"] = sha256(src)
    for fixture in sorted(FIXTURES.iterdir()):
        if not fixture.is_file():
            continue
        dest = f"{GUEST_SRC_DIR}/{fixture.name}"
        commands.append(f"write {fixture} {dest}")
        hashes[f"fixtures/{fixture.name}"] = sha256(fixture)
    commands.append(f"write {host_archive} {GUEST_WORK_DIR}/host-index-input.a")
    hashes["host-archive-input.a"] = sha256(host_archive)
    output = run_debugfs(image, commands, command_file)
    if "File not found" in output or "command not found" in output.lower():
        raise RuntimeError(f"debugfs import error:\n{output}")
    return hashes


def dump_guest_file(image: Path, guest_path: str, destination: Path) -> None:
    proc = subprocess.run(["debugfs", "-R", f"dump {guest_path} {destination}",
                           str(image)], text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, check=False)
    if not destination.is_file():
        raise RuntimeError(f"could not extract {guest_path}: {proc.stdout}")


def boot(qemu: Path, kernel: Path, image: Path, log_path: Path,
         command: str, timeout: int, qemu_log_path: Path) -> tuple[str, bool]:
    qemu_args = ["-M", "myemulator32", "-m", "128M", "-kernel", str(kernel),
                 "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk",
                 "-nographic", "-monitor", "none", "-serial", "stdio",
                 "-icount", "shift=0,sleep=off"]
    output_parts: list[str] = []
    complete = False
    with log_path.open("w", encoding="utf-8", errors="replace") as log, \
            qemu_log_path.open("w", encoding="utf-8", errors="replace") as qlog:
        child = None
        try:
            # Do not create a new process group here: the managed sandbox
            # denies setsid, while QEMU itself is allowed to run normally.
            child = pexpect.spawn(str(qemu), qemu_args, timeout=timeout,
                                  encoding="utf-8", codec_errors="replace")
            child.logfile = log
            child.logfile_read = qlog
            child.expect(PROMPT, timeout=min(timeout, 180))

            def run_guest(line: str, marker: str) -> tuple[str, str]:
                child.sendline(f"{line}; echo {marker}:$?")
                child.expect(rf"{marker}:(\d+)", timeout=timeout)
                rc = child.match.group(1)
                before = child.before
                child.expect(PROMPT, timeout=timeout)
                return rc, before

            phase_rc, phase_output = run_guest(command, "BNQ_PHASE_RC")
            output_parts.append(phase_output)
            sync_rc, sync_output = run_guest("sync", "BNQ_SYNC_RC")
            output_parts.append(sync_output)
            output_parts.append(f"\nBNQ_PHASE_STATUS:{phase_rc}\nBNQ_SYNC_STATUS:{sync_rc}\n")
            complete = phase_rc == "0" and sync_rc == "0"
        except (pexpect.TIMEOUT, pexpect.EOF, OSError) as exc:
            output_parts.append(f"\n{type(exc).__name__}: {exc}\n")
        finally:
            # Explicit sync above, then stop QEMU and boot the same disposable
            # ext4 image again. This is the flight development persistence
            # smoke; it never touches the source or production rootfs.
            if child is not None:
                try:
                    child.close(force=True)
                except Exception:
                    pass
    return "".join(output_parts), not complete


def collect_guest_files(image: Path, out: Path, names: list[str]) -> dict[str, str]:
    collected: dict[str, str] = {}
    destdir = out / "guest-output"
    destdir.mkdir()
    for name in names:
        dest = destdir / name.replace("/", "_")
        guest = f"/root/binqual/work/{name}"
        proc = subprocess.run(["debugfs", "-R", f"dump {guest} {dest}", str(image)],
                              text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, check=False)
        if dest.is_file():
            collected[name] = dest.read_text(encoding="utf-8", errors="replace")
    return collected


def elf_identity(path: Path) -> str:
    reader = ROOT / ".toolchain-install/bin/myemulator2-elf-readelf"
    proc = subprocess.run([str(reader), "-h", str(path)], text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          check=False)
    return proc.stdout


def inspect_outputs(files: dict[str, str]) -> dict[str, bool]:
    checks = {
        "readelf_object_header_machine": "MyEmulator2" in files.get("single.header", ""),
        "readelf_object_sections_symbols_relocs": all(
            term in files.get("single.sections", "")
            for term in (".bss", "scratch", "R_MYEMULATOR2")),
        "objdump_object_disassembly": all(
            term in files.get("single.dis", "") for term in ("syscall", "lui")),
        "readelf_executable_program_headers": all(
            term in files.get("single.exe.readelf", "") for term in ("LOAD", "scratch")),
        "objdump_executable_disassembly": all(
            term in files.get("single.exe.dis", "") for term in ("syscall", "jr r14")),
        "readelf_external_relocation": "answer" in files.get("multi.relocs", ""),
        "native_ld_relocatable_output": "MyEmulator2" in files.get(
            "ld-hello-rel.header", ""),
        "ar_archive_members": all(
            term in files.get("archive.list", "") for term in ("archive-a.o", "archive-b.o")),
        "readelf_archive_member_symbol": "value_a" in files.get("archive.member.readelf", ""),
        "readelf_post_restart": "MyEmulator2" in files.get("post.archive.header", ""),
        "objdump_post_restart_instruction": "syscall" in files.get("post.archive.dis", ""),
        "undefined_symbol_diagnostic": any(
            term in files.get("unresolved.log", "").lower()
            for term in ("undefined", "unresolved", "not defined")),
    }
    return checks


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rootfs", type=Path,
                        default=PACKAGE / "disk/rem-rootfs.ext4")
    parser.add_argument("--qemu", type=Path,
                        default=PACKAGE / "qemu/qemu-system-myemulator32")
    parser.add_argument("--kernel", type=Path, default=PACKAGE / "boot/vmlinux")
    parser.add_argument("--output-dir", type=Path,
                        default=Path("/tmp/rem-native-binutils-qualification"))
    parser.add_argument("--guest-script", default="guest-qualification.sh",
                        help="guest fixture script basename to run")
    parser.add_argument("--post-reboot-script", default="guest-post-reboot.sh",
                        help="guest persistence script basename to run")
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()
    rootfs = args.rootfs.resolve()
    qemu = args.qemu.resolve()
    kernel = args.kernel.resolve()
    out = args.output_dir.resolve()
    for script_name in (args.guest_script, args.post_reboot_script):
        if Path(script_name).name != script_name or not (FIXTURES / script_name).is_file():
            raise SystemExit(f"guest script must name a fixture basename: {script_name}")
    for path in (rootfs, qemu, kernel):
        if not path.is_file():
            raise SystemExit(f"missing tuple artifact: {path}")
    if out.exists():
        raise SystemExit(f"refusing to overwrite existing output directory: {out}")
    out.mkdir(parents=True)
    image = out / "binutils-qualification.ext4"
    native_ld_rootfs = out / "native-root-ld"
    try:
        dump_guest_file(rootfs, "/root/native/bin/ld", native_ld_rootfs)
    except Exception as exc:
        raise SystemExit(f"missing native root linker artifact: {exc}")
    native_root_ld_sha = sha256(native_ld_rootfs)
    if native_root_ld_sha != EXPECTED_CORE["ld"]:
        raise SystemExit(f"unexpected /root/native/bin/ld hash: {native_root_ld_sha}")
    subprocess.run(["cp", "--reflink=auto", "--sparse=always", str(rootfs), str(image)],
                   check=True)
    try:
        host_archive = prepare_host_archive(out)
        imported = inject(image, out, out / "debugfs.commands", host_archive)
    except Exception as exc:
        print(f"FAIL staging disposable guest: {exc}", file=sys.stderr)
        return 2

    pre_command = ("chmod 0755 /root/binqual/bin/* && "
                   f"sh /root/binqual/src/{args.guest_script}; "
                   "echo HOST_PHASE_EXIT:$?")
    pre_output, pre_timeout = boot(qemu, kernel, image, out / "phase1.console.log",
                                   pre_command, args.timeout, out / "phase1.qemu.log")
    print(pre_output)
    pre_marker = ("BINUTILS_GUEST_FAILURES:0" in pre_output and
                  "HOST_PHASE_EXIT:0" in pre_output and
                  "BNQ_PHASE_STATUS:0" in pre_output and "BNQ_SYNC_STATUS:0" in pre_output)
    if not pre_marker:
        print("FAIL native guest qualification phase")

    post_command = (f"sh /root/binqual/src/{args.post_reboot_script}; "
                    "echo POST_REBOOT_EXIT:$?")
    post_output, post_timeout = boot(qemu, kernel, image, out / "phase2.console.log",
                                     post_command, args.timeout, out / "phase2.qemu.log")
    print(post_output)
    post_marker = ("BINUTILS_POST_REBOOT_FAILURES:0" in post_output and
                   "POST_REBOOT_EXIT:0" in post_output and
                   "BNQ_PHASE_STATUS:0" in post_output and "BNQ_SYNC_STATUS:0" in post_output)
    if not post_marker:
        print("FAIL native guest post-reboot qualification")

    guest_files = collect_guest_files(image, out, [
        "single.header", "single.sections", "single.dis", "single.exe.readelf",
        "single.exe.dis", "multi.relocs", "archive.list", "archive.member.readelf",
        "ld-empty-rel.log", "ld-hello-rel.log", "ld-hello-link.log",
        "ld-hello-rel.header", "legacy-path-ld.log",
        "unresolved.log", "post.archive.header", "post.archive.dis",
        "ar-create.log", "ranlib-create.log", "archive-link.log",
        "ar-extract.log", "archive-b-update.log", "ar-update.log",
        "ranlib-update.log", "archive-relink.log", "hello-cc.log",
        "archive-c-cc.log", "archive-c-ar.log", "archive-c-ld.log",
        "invalid.log", "duplicate.log", "bad-ld.log", "missing-ar.log",
        "host-ranlib.log", "host-archive.readelf",
    ])
    output_checks = inspect_outputs(guest_files)
    for name, passed in output_checks.items():
        print(f"{'PASS' if passed else 'FAIL'} host-inspected {name}")

    artifact_hashes = {name: sha256(STAGING / "bin" / name) for name in TEST_TOOLS}
    artifact_sizes = {name: (STAGING / "bin" / name).stat().st_size for name in TEST_TOOLS}
    hash_matches = {name: artifact_hashes[name] == expected
                    for name, expected in EXPECTED_CORE.items()}
    elf_identities = {name: elf_identity(STAGING / "bin" / name) for name in TEST_TOOLS}
    report = {
        "status": "PASS" if pre_marker and post_marker and not pre_timeout and
                  not post_timeout and all(hash_matches.values()) and
                  all(output_checks.values()) else "FAIL",
        "tuple": {"qemu": str(qemu), "qemu_sha256": sha256(qemu),
                  "kernel": str(kernel), "kernel_sha256": sha256(kernel),
                  "rootfs_source": str(rootfs),
                  "rootfs_source_sha256_pinned": PINNED_ROOTFS_SHA256,
                  "test_rootfs": str(image), "test_rootfs_bytes": image.stat().st_size},
        "native_tools_sha256": artifact_hashes,
        "native_tools_sizes": artifact_sizes,
        "native_tools_elf_headers": elf_identities,
        "native_root_linker": {"guest_path": "/root/native/bin/ld",
                               "sha256": native_root_ld_sha,
                               "bytes": native_ld_rootfs.stat().st_size},
        "legacy_path_ld_crashed_on_r":
            "OBSERVED legacy PATH ld crash on -r" in pre_output,
        "tuple_expected_hash_match": hash_matches,
        "host_inspection_checks": output_checks,
        "imported_guest_files_sha256": imported,
        "phase1_timeout": pre_timeout,
        "phase2_timeout": post_timeout,
        "persistence_restart_method": "guest sync, host QEMU stop, then boot the same disposable rootfs image again",
        "guest_outputs": str(out / "guest-output"),
        "phase1_log": str(out / "phase1.console.log"),
        "phase2_log": str(out / "phase2.console.log"),
        "archive_faults": [line.strip() for line in
                            (out / "phase1.console.log").read_text(
                                encoding="utf-8", errors="replace").splitlines()
                            if "MYEMU_BAD_USER_FAULT" in line and
                            ("comm=ar " in line or "comm=ranlib " in line)],
    }
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n",
                                     encoding="utf-8")
    print(f"BINUTILS_QUALIFICATION:{report['status']}")
    print(f"REPORT:{out / 'result.json'}")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
