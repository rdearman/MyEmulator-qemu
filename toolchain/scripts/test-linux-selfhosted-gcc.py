#!/usr/bin/env python3
"""Run the Canadian REM GCC itself and compile a program inside the guest."""

import os
import json
import signal
import shutil
import socket
import stat
import subprocess
import tempfile
import time
import shlex
from pathlib import Path
from linux_build_config import temporary_linux_config

ROOT = Path(__file__).resolve().parents[2]
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32",
                            ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"
GEN_INIT_CPIO = ROOT / ".linux-build/build/usr/gen_init_cpio"
LINUX_BUILD = ROOT / ".linux-build/build"
CROSS_GCC = Path(os.environ.get(
    "MYEMU_HOST_CROSS_GCC",
    "/tmp/rem-native-gcc-hosted-cxx/bin/myemulator2-linux-musl-gcc"))
NATIVE_PREFIX = Path(os.environ.get(
    "MYEMU_NATIVE_GCC_PREFIX", "/tmp/rem-native-gcc-guest-bootstrap2"))
NATIVE_BINUTILS = Path(os.environ.get(
    "MYEMU_NATIVE_BINUTILS", "/tmp/rem-native-binutils-far"))
TARGET_SYSROOT = Path(os.environ.get(
    "MYEMU_NATIVE_SYSROOT", "/tmp/rem-native-gcc-hosted-cxx/myemulator2-linux-musl"))
STRIP = ROOT / ".toolchain-install/bin/myemulator2-elf-strip"


def _pdeathsig():
    import ctypes
    ctypes.CDLL(None).prctl(1, signal.SIGTERM, 0, 0, 0)


def _listing(root: Path, host_root: Path) -> str:
    lines = ["dir / 0755 0 0", "dir /dev 0755 0 0",
             "nod /dev/console 0600 0 0 c 5 1",
             "nod /dev/ttyMY0 0600 0 0 c 240 0"]
    for path in sorted(root.rglob("*")):
        rel = "/" + path.relative_to(root).as_posix()
        mode = stat.S_IMODE(path.lstat().st_mode)
        if path.is_symlink():
            lines.append(f"slink {rel} {os.readlink(path)} 0777 0 0")
        elif path.is_dir():
            lines.append(f"dir {rel} {mode:04o} 0 0")
        elif path.is_file():
            lines.append(f"file {rel} {host_root / path.relative_to(root)} {mode:04o} 0 0")
    return "\n".join(lines) + "\n"


def _qmp_command(qmp, command, arguments=None):
    payload = {"execute": command}
    if arguments is not None:
        payload["arguments"] = arguments
    qmp.sendall((__import__("json").dumps(payload) + "\r\n").encode())
    qmp.settimeout(0.5)
    response = b""
    while b"\r\n" not in response:
        response += qmp.recv(4096)
    return __import__("json").loads(response.split(b"\r\n", 1)[0])


def _stage_file(source: Path, destination: Path, strip=False):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    if strip and STRIP.exists():
        # Keep the initramfs fixture below the machine's 16 MiB RAM budget.
        # The diagnostic only needs executable contents; debug and ordinary
        # symbol tables are not needed to establish guest execution.
        subprocess.run([str(STRIP), "--strip-all", str(destination)],
                       check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)


def _stage_tree(source: Path, destination: Path):
    if source.is_dir():
        for path in source.rglob("*"):
            if path.is_file():
                _stage_file(path, destination / path.relative_to(source))


def main():
    for path in (QEMU, KERNEL, GEN_INIT_CPIO, CROSS_GCC,
                 NATIVE_PREFIX / "bin/gcc", NATIVE_BINUTILS / "bin/as",
                 TARGET_SYSROOT / "lib/libc.a"):
        if not path.exists():
            raise SystemExit(f"missing self-hosted GCC input: {path}")

    with tempfile.TemporaryDirectory(prefix="myemu-selfhost-gcc-") as tmp_name:
        tmp = Path(tmp_name)
        gcc_args = shlex.split(os.environ.get(
            "MYEMU_SELFHOST_GCC_ARGS", "-O0 /hello.c -o /hello"))
        rootfs = tmp / "root"
        rootfs.mkdir()
        # The Canadian install prefix also contains the GCC build tree.  A
        # recursive copy embeds gigabytes of sources and unstripped compiler
        # binaries into initramfs.  Stage only the files the guest driver
        # needs, stripping compiler/debug payloads to keep the fixture
        # bootable and bounded.
        native_root = rootfs / "tmp/rem-native-gcc-guest-bootstrap2"
        for rel in ("bin/gcc", "bin/c++", "bin/cpp", "bin/g++",
                    "bin/myemulator2-linux-musl-gcc",
                    "bin/myemulator2-linux-musl-gcc-15.2.0"):
            source_path = NATIVE_PREFIX / rel
            if source_path.exists() and source_path.is_file():
                _stage_file(source_path, native_root / rel, strip=True)
        if gcc_args != ["--version"]:
            gcc_version = NATIVE_PREFIX / "libexec/gcc/myemulator2-linux-musl/15.2.0"
            for name in ("cc1", "cc1plus", "collect2", "lto-wrapper"):
                source_path = gcc_version / name
                if source_path.exists():
                    _stage_file(source_path,
                                native_root / source_path.relative_to(NATIVE_PREFIX),
                                strip=True)
            _stage_tree(NATIVE_PREFIX / "lib/gcc",
                        native_root / "lib/gcc")

            sysroot_root = rootfs / "tmp/rem-native-gcc-hosted-cxx"
            _stage_tree(TARGET_SYSROOT,
                        sysroot_root / TARGET_SYSROOT.relative_to(TARGET_SYSROOT.parent))

            binutils_root = rootfs / "tmp/rem-native-binutils-far"
            for source_path in (NATIVE_BINUTILS / "bin").iterdir():
                if source_path.is_file():
                    _stage_file(source_path,
                                binutils_root / "bin" / source_path.name,
                                strip=True)

        source = tmp / "runner.c"
        argv_source = ", ".join(json.dumps(arg) for arg in gcc_args)
        source.write_text(r"""#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
int main(void) {
  int fd = open("/dev/console", O_RDWR);
  if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); }
  write(1, "RUNNER_START\\n", 13);
  char *argv[] = {"gcc", """ + argv_source + r""", (char *)0};
  char *envp[] = {
    "PATH=/tmp/rem-native-gcc-guest-bootstrap2/bin:/tmp/rem-native-binutils-far/bin",
    "COMPILER_PATH=/tmp/rem-native-binutils-far/bin",
    (char *)0
  };
  pid_t child = fork();
  if (child < 0) { perror("fork"); return 112; }
  if (child == 0) {
    execve("/tmp/rem-native-gcc-guest-bootstrap2/bin/gcc", argv, envp);
    perror("exec gcc");
    _exit(111);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0) { perror("waitpid"); return 113; }
  if (WIFEXITED(status)) {
    char line[64];
    int n = snprintf(line, sizeof(line), "CHILD_EXIT:%d\\n", WEXITSTATUS(status));
    write(1, line, n);
  } else if (WIFSIGNALED(status)) {
    char line[64];
    int n = snprintf(line, sizeof(line), "CHILD_SIGNAL:%d\\n", WTERMSIG(status));
    write(1, line, n);
  } else {
    write(1, "CHILD_UNKNOWN\\n", 15);
  }
  return 0;
}
""")
        runner = tmp / "runner"
        subprocess.run([str(CROSS_GCC), str(source), "-static", "-T",
                        str(ROOT / "toolchain/userspace/myemulator2-user.ld"),
                        "-o", str(runner)],
                       check=True)
        saved_runner = os.environ.get("MYEMU_SAVE_RUNNER")
        if saved_runner:
            shutil.copy2(runner, saved_runner)
        use_ext4 = os.environ.get("MYEMU_SELFHOST_EXT4", "1") != "0"
        # The development kernel is often built with a regression fixture
        # embedded in CONFIG_INITRAMFS_SOURCE.  Do not collide with its
        # /init: an external initrd is merged with the built-in archive, and
        # the built-in process-test /init can otherwise win.  Select this
        # runner explicitly with rdinit so this test always exercises the
        # disposable guest-native toolchain fixture.
        if use_ext4:
            (rootfs / "sbin").mkdir(parents=True, exist_ok=True)
            (rootfs / "sbin/init").write_bytes(runner.read_bytes())
            (rootfs / "sbin/init").chmod(0o755)
            (rootfs / "dev").mkdir(exist_ok=True)
        else:
            (rootfs / "selfhost-init").write_bytes(runner.read_bytes())
            (rootfs / "selfhost-init").chmod(0o755)
        (rootfs / "hello.c").write_text(
            '#include <stdio.h>\nint main(void) { puts("HELLO_FROM_GUEST_GCC"); return 0; }\n')
        image = None
        if use_ext4:
            image = tmp / "selfhost-root.img"
            subprocess.run(["truncate", "-s", os.environ.get(
                "MYEMU_SELFHOST_EXT4_SIZE", "128M"), str(image)], check=True)
            if shutil.which("fakeroot"):
                shell = ("mknod -m 600 {root}/dev/console c 5 1; "
                         "mknod -m 600 {root}/dev/ttyMY0 c 240 0; "
                         "mkfs.ext4 -q -F -d {root} {image}").format(
                             root=shlex.quote(str(rootfs)),
                             image=shlex.quote(str(image)))
                subprocess.run(["fakeroot", "sh", "-c", shell], check=True)
            else:
                subprocess.run(["mkfs.ext4", "-q", "-F", "-d", str(rootfs),
                                str(image)], check=True)
        else:
            listing = tmp / "init.list"
            listing.write_text(_listing(rootfs, rootfs))
            initrd = tmp / "initramfs.cpio"
            with initrd.open("wb") as output:
                subprocess.run([str(GEN_INIT_CPIO), str(listing)],
                               check=True, stdout=output)

        linux_scripts_config = ROOT / ".linux-build/linux-6.12.1/scripts/config"
        with temporary_linux_config(LINUX_BUILD / ".config"):
            if use_ext4:
                subprocess.run([str(linux_scripts_config), "--file",
                                str(LINUX_BUILD / ".config"), "--set-str",
                                "INITRAMFS_SOURCE", ""], check=True)
                boot_args = "console=ttyMY0 root=/dev/myemu0 rw init=/sbin/init"
            else:
                subprocess.run([str(linux_scripts_config), "--file",
                                str(LINUX_BUILD / ".config"), "--set-str",
                                "INITRAMFS_SOURCE", str(initrd)], check=True)
                boot_args = "console=ttyMY0 rdinit=/selfhost-init"
            subprocess.run([str(linux_scripts_config), "--file",
                            str(LINUX_BUILD / ".config"), "--set-str",
                            "CMDLINE", boot_args],
                           check=True)
            subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")],
                           check=True, stdout=subprocess.DEVNULL)

        sock_path = tmp / "console.sock"
        qmp_path = tmp / "qmp.sock"
        command = [
            str(QEMU), "-M", "myemulator32", "-m",
            os.environ.get("MYEMU_SELFHOST_RAM", "16M"),
            "-kernel", str(KERNEL),
            "-append", boot_args,
            "-nographic", "-monitor", "none",
            "-serial", "chardev:console",
            "-chardev", f"socket,id=console,path={sock_path},server=on,wait=on",
            "-qmp", f"unix:{qmp_path},server=on,wait=off",
            "-icount", os.environ.get("MYEMU_ICOUNT", "shift=0,sleep=off")]
        if image is not None:
            command[command.index("-nographic"):command.index("-nographic")] = [
                "-drive", f"file={image},format=raw,if=none,id=myemulator2-disk"]
        else:
            command[command.index("-nographic"):command.index("-nographic")] = [
                "-initrd", str(initrd)]
        extra_qemu = os.environ.get("MYEMU_QEMU_EXTRA")
        if extra_qemu:
            command[1:1] = shlex.split(extra_qemu)
        proc = subprocess.Popen(command,
            stdout=(qemu_log := open(os.environ["MYEMU_SAVE_QEMU_LOG"], "wb"))
                if os.environ.get("MYEMU_SAVE_QEMU_LOG") else subprocess.DEVNULL,
            stderr=qemu_log if os.environ.get("MYEMU_SAVE_QEMU_LOG") else subprocess.DEVNULL,
            start_new_session=True, preexec_fn=_pdeathsig)
        sock = None
        qmp = None
        try:
            for _ in range(300):
                if sock_path.exists():
                    try:
                        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                        sock.connect(str(sock_path))
                        break
                    except OSError:
                        if sock is not None:
                            sock.close()
                        sock = None
                time.sleep(0.05)
            if sock is None:
                raise RuntimeError("QEMU console socket did not open")
            for _ in range(300):
                if qmp_path.exists():
                    try:
                        qmp = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                        qmp.connect(str(qmp_path))
                        qmp.settimeout(1.0)
                        qmp.recv(4096)  # greeting
                        _qmp_command(qmp, "qmp_capabilities")
                        break
                    except OSError:
                        if qmp is not None:
                            qmp.close()
                        qmp = None
                time.sleep(0.05)
            sock.settimeout(0.2)
            data = b""
            last_sample = 0.0
            deadline = time.monotonic() + float(
                os.environ.get("MYEMU_SELFHOST_GCC_TIMEOUT", "90"))
            expected = os.environ.get("MYEMU_SELFHOST_EXPECT", "HELLO_FROM_GUEST_GCC").encode()
            while expected not in data and time.monotonic() < deadline:
                now = time.monotonic()
                if qmp is not None and now - last_sample >= 5.0:
                    try:
                        sample = _qmp_command(qmp, "myemulator32-debug",
                                              {"op": "regs"})
                        regs = sample.get("return", {})
                        frame = None
                        if regs.get("ssp"):
                            mem = _qmp_command(qmp, "myemulator32-debug", {
                                "op": "physmem", "address": regs["ssp"],
                                "length": 16})
                            frame = mem.get("return", {}).get("data")
                        print("SELFHOST_PROGRESS", regs, "frame=", frame,
                              flush=True)
                    except (OSError, TimeoutError, ValueError):
                        qmp = None
                    last_sample = now
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            if expected not in data:
                saved_serial = os.environ.get("MYEMU_SAVE_SERIAL")
                if saved_serial:
                    Path(saved_serial).write_bytes(data)
                raise RuntimeError("guest-native GCC did not compile/run: " +
                                   repr(data[-4000:]))
        finally:
            if sock is not None:
                sock.close()
            if qmp is not None:
                qmp.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait(timeout=5)
            if os.environ.get("MYEMU_SAVE_QEMU_LOG"):
                qemu_log.close()
    print("REM guest-native GCC: PASS")


if __name__ == "__main__":
    main()
