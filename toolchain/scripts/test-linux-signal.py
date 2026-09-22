#!/usr/bin/env python3
"""Static/build checks for the MyEmulator2 Linux signal ABI."""

from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
ARCH = ROOT / "linux/arch/myemulator2"
BUILD = ROOT / ".linux-build/build"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def main() -> None:
    signal = (ARCH / "kernel/signal.c").read_text()
    traps = (ARCH / "kernel/traps.c").read_text()
    entry = (ARCH / "kernel/entry.S").read_text()
    syscall = (ARCH / "kernel/syscall.c").read_text()
    ptrace = (ARCH / "include/asm/ptrace.h").read_text()

    for symbol in (
        "arch_do_signal_or_restart",
        "myemulator2_rt_sigreturn",
        "setup_rt_frame",
        "restore_sigcontext",
        "get_signal(&ksig)",
        "signal_setup_done",
        "set_current_blocked",
        "restore_altstack",
    ):
        require(symbol in signal, f"signal implementation lacks {symbol}")
    for syscall_name in ("MYEMU2_NR_KILL", "MYEMU2_NR_TKILL", "MYEMU2_NR_TGKILL", "MYEMU2_NR_SIGALTSTACK"):
        require(syscall_name in syscall, f"signal syscall dispatcher lacks {syscall_name}")
    require("orig_r1" in ptrace, "pt_regs has no saved syscall number")
    require("addi sp, sp, -96" in entry and "addi sp, sp, 96" in entry,
            "exception entry does not reserve space for the extended pt_regs")
    require("arch_do_signal_or_restart(regs)" in traps,
            "exception return does not run pending signal work")
    require("#define __NR_rt_sigreturn 139" in (ARCH / "include/uapi/asm/unistd.h").read_text(),
            "rt_sigreturn number is not exported")

    vmlinux = BUILD / "vmlinux"
    require(vmlinux.exists(), f"isolated kernel build is missing {vmlinux}")
    nm = subprocess.run([".toolchain-install/bin/myemulator2-elf-nm", str(vmlinux)],
                        cwd=ROOT, check=True, text=True, capture_output=True).stdout
    for symbol in ("arch_do_signal_or_restart", "myemulator2_rt_sigreturn"):
        require(re.search(rf"\b{re.escape(symbol)}$", nm, re.MULTILINE) is not None,
                f"built kernel does not contain {symbol}")
    print("PASS: MyEmulator2 signal ABI source and isolated vmlinux checks")


if __name__ == "__main__":
    main()
