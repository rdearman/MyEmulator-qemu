#!/usr/bin/env python3
"""Small functional regression suite for the maintained MyEmulator2 port."""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time


def run(command, *, cwd=None):
    result = subprocess.run(command, cwd=cwd, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise AssertionError(f"{command!r} failed:\n{result.stderr}")
    return result


class QMP:
    def __init__(self, path: Path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        deadline = time.time() + 5
        while True:
            try:
                self.sock.connect(str(path))
                break
            except OSError:
                if time.time() >= deadline:
                    raise
                time.sleep(.01)
        self.sock.settimeout(5)
        self.buf = b""
        self.read()
        self.command("qmp_capabilities")

    def read(self):
        while b"\n" not in self.buf:
            self.buf += self.sock.recv(4096)
        line, self.buf = self.buf.split(b"\n", 1)
        return json.loads(line)

    def command(self, name, arguments=None):
        message = {"execute": name, "id": name}
        if arguments is not None:
            message["arguments"] = arguments
        self.sock.sendall((json.dumps(message) + "\n").encode())
        while True:
            reply = self.read()
            if reply.get("id") == name:
                return reply

    def hmp(self, command):
        return self.command("human-monitor-command", {"command-line": command})[
            "return"]

    def close(self):
        self.sock.close()


def run_elf(qemu, elf: Path, expected: int, register="R1"):
    with tempfile.TemporaryDirectory(prefix="myemu2-qemu-") as name:
        qmp_path = Path(name) / "qmp.sock"
        proc = subprocess.Popen([qemu, "-machine", "myemulator32", "-display",
                                 "none", "-kernel", str(elf), "-S", "-qmp",
                                 f"unix:{qmp_path},server=on,wait=off"],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        monitor = None
        try:
            monitor = QMP(qmp_path)
            monitor.command("cont")
            deadline = time.time() + 5
            output = ""
            while time.time() < deadline:
                output = monitor.hmp("info registers")
                if "halted=1" in output:
                    break
                time.sleep(.01)
            assert "halted=1" in output, output
            match = re.search(rf"{register}: 0x([0-9a-f]+)", output)
            assert match and int(match.group(1), 16) == expected, output
        finally:
            if monitor:
                monitor.close()
            proc.terminate()
            proc.wait(timeout=5)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: test-binutils.py BINUTILS-BIN ROOT")
    bindir = Path(sys.argv[1]).resolve()
    root = Path(sys.argv[2]).resolve()
    tool = lambda name: str(bindir / f"myemulator2-elf-{name}")
    qemu = os.environ.get("QEMU_MYEMULATOR32", str(root / ".qemu-build" /
                                                   "qemu-system-myemulator32"))

    source = """\
.text
.global _start
_start:
        addi sp, sp, -16
        sw lr, 12(sp)
        li r1, 5
        li r2, 7
        call add_values
        lw lr, 12(sp)
        addi sp, sp, 16
        lui r5, %hi(answer)
        ori r5, r5, %lo(answer)
        sw r3, 0(r5)
        lw r4, 0(r5)
        beq r4, r3, done
        halt
done:
        halt

add_values:
        add r3, r1, r2
        ret

.data
.align 4
answer:
        .word 0

.section .rodata
message:
        .asciz "MyEmulator2\\n"
"""

    with tempfile.TemporaryDirectory(prefix="myemu2-binutils-") as name:
        directory = Path(name)
        asm = directory / "program.s"
        obj = directory / "program.o"
        elf = directory / "program.elf"
        raw = directory / "program.bin"
        asm.write_text(source)
        run([tool("as"), "-o", str(obj), str(asm)])
        rel = run([tool("readelf"), "-r", str(obj)]).stdout
        assert "R_MYEMULATOR2_" in rel, rel
        run([tool("ld"), "-o", str(elf), str(obj)])
        header = run([tool("readelf"), "-h", str(elf)]).stdout
        assert "ELF32" in header and "MyEmulator2" in header, header
        listing = run([tool("objdump"), "-d", str(elf)]).stdout
        for mnemonic in ("addi", "add", "jal", "sw", "lw", "beq", "halt"):
            assert mnemonic in listing.lower(), listing
        run([tool("nm"), str(elf)])
        run([tool("objcopy"), "-O", "binary", str(elf), str(raw)])
        assert raw.stat().st_size >= 4

        if not Path(qemu).exists():
            print("binutils functional tests: PASS (QEMU unavailable; execution skipped)")
            return
        run_elf(qemu, elf, 12, "R3")

        # Cross-object call and data relocation.  This also exercises a
        # separately assembled .bss section and a global symbol.
        main_s = directory / "main.s"
        funcs_s = directory / "functions.s"
        main_o = directory / "main.o"
        funcs_o = directory / "functions.o"
        multi = directory / "multi.elf"
        main_s.write_text("""\
.text
.global _start
_start:
        li r1, value
        lw r1, 0(r1)
        call increment
        halt
.data
value:
        .word 41
.section .bss
scratch:
        .space 4
""")
        funcs_s.write_text("""\
.text
.global increment
increment:
        addi r1, r1, 1
        ret
""")
        run([tool("as"), "-o", str(main_o), str(main_s)])
        run([tool("as"), "-o", str(funcs_o), str(funcs_s)])
        run([tool("ld"), "-o", str(multi), str(main_o), str(funcs_o)])
        symbols = run([tool("nm"), str(multi)]).stdout
        assert "increment" in symbols and "value" in symbols
        run_elf(qemu, multi, 42)

        # Relocation-stress fixture: explicit HI20/LO12 pairs, branch and
        # jump relocations, addends in data, and forward/backward labels.
        stress_o = directory / "stress.o"
        stress = directory / "stress.elf"
        stress_source = root / "toolchain/examples/relocation-stress.s"
        run([tool("as"), "-o", str(stress_o), str(stress_source)])
        run([tool("ld"), "-o", str(stress), str(stress_o)])
        stress_listing = run([tool("objdump"), "-d", str(stress)]).stdout.lower()
        for mnemonic in ("lui", "ori", "beq", "jal", "halt"):
            assert mnemonic in stress_listing, stress_listing
        print("binutils functional tests: PASS")


if __name__ == "__main__":
    main()
