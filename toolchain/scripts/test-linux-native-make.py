#!/usr/bin/env python3
"""Execute a statically linked GNU Make target inside MyEmulator2 Linux."""

import os
import signal
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GCC = Path(os.environ.get("MYEMU_NATIVE_GCC", "/tmp/myemu-native/bin/myemulator2-linux-musl-gcc"))
MAKE = Path(os.environ.get("MYEMU_NATIVE_MAKE", "/tmp/myemu-make-build/make"))
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))
KERNEL = ROOT / ".linux-build/build/vmlinux"
LINUX = ROOT / ".linux-build/linux-6.12.1"
BUILD = ROOT / ".linux-build/build"
GEN_INIT_CPIO = ROOT / ".linux-build/build/usr/gen_init_cpio"
INITRAMFS = Path("/tmp/myemu-busybox-final-initramfs/initramfs.cpio")


def _child_death_signal():
    """Ensure an externally interrupted harness cannot orphan its QEMU."""
    import ctypes
    libc = ctypes.CDLL(None)
    libc.prctl(1, signal.SIGTERM, 0, 0, 0)  # PR_SET_PDEATHSIG


def main():
    for path in (GCC, MAKE, QEMU, KERNEL, GEN_INIT_CPIO):
        if not path.exists():
            raise SystemExit(f"missing required file: {path}")
    with tempfile.TemporaryDirectory(prefix="myemu-native-make-") as tmp_name:
        tmp = Path(tmp_name)
        runner = tmp / "runner.c"
        init = tmp / "init"
        runner.write_text(r'''#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
int main(void) {
    char *const argv[] = { (char *)"make", (char *)"-f", (char *)"/Makefile", NULL };
    char *const envp[] = { (char *)"PATH=/bin", NULL };
    int console = open("/dev/console", O_RDWR);
    if (console >= 0) {
        dup2(console, STDIN_FILENO);
        dup2(console, STDOUT_FILENO);
        dup2(console, STDERR_FILENO);
    }
    execve("/bin/make", argv, envp);
    perror("execve make");
    return 111;
}
''')
        env = os.environ.copy()
        env["PATH"] = "/tmp/myemu-native/bin:/tmp/myemu-binutils-install/bin:" + env.get("PATH", "")
        subprocess.run([str(GCC), str(runner), "-o", str(init)], check=True, env=env)
        listing = tmp / "init.list"
        listing.write_text(
            "dir /bin 0755 0 0\n"
            "dir /dev 0755 0 0\n"
            f"file /init {init} 0755 0 0\n"
            f"file /bin/make {MAKE} 0755 0 0\n"
            "file /Makefile " + str(tmp / "Makefile") + " 0644 0 0\n"
            "nod /dev/console 0600 0 0 c 5 1\n"
            "nod /dev/ttyMY0 0600 0 0 c 240 0\n")
        # The info function exercises Make's parser and output path without
        # requiring a shell applet in this minimal initramfs.
        (tmp / "Makefile").write_text("$(info native-make-pass)\nall:\n")
        INITRAMFS.parent.mkdir(parents=True, exist_ok=True)
        with INITRAMFS.open("wb") as out:
            subprocess.run([str(GEN_INIT_CPIO), str(listing)], check=True, stdout=out)
        subprocess.run([str(LINUX / "scripts/config"), "--file",
                        str(BUILD / ".config"), "--set-str",
                        "INITRAMFS_SOURCE", str(INITRAMFS)], check=True)
        subprocess.run([str(ROOT / "toolchain/scripts/build-linux.sh")], check=True, stdout=subprocess.DEVNULL)
        sock_path = tmp / "console.sock"
        qemu_args = [
            str(QEMU), "-M", "myemulator32", "-m", "16M", "-kernel", str(KERNEL),
            "-nographic", "-monitor", "none",
            "-serial", "chardev:console", "-chardev", f"socket,id=console,path={sock_path},server=on,wait=on"]
        if not os.environ.get("MYEMU_NATIVE_MAKE_NO_ICOUNT"):
            qemu_args += ["-icount", "shift=0,sleep=off"]
        proc = subprocess.Popen(qemu_args, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, start_new_session=True,
            preexec_fn=_child_death_signal)
        sock = None
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
            sock.settimeout(0.2)
            data = b""
            deadline = time.monotonic() + float(os.environ.get("MYEMU_NATIVE_MAKE_TIMEOUT", "45"))
            while b"native-make-pass" not in data and time.monotonic() < deadline:
                try:
                    data += sock.recv(8192)
                except socket.timeout:
                    pass
            if b"native-make-pass" not in data:
                raise RuntimeError(f"GNU Make did not execute its guest target; serial={data[-2000:]!r}")
        finally:
            if sock is not None:
                sock.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
    print("MyEmulator2 Linux native GNU Make: PASS")


if __name__ == "__main__":
    main()
