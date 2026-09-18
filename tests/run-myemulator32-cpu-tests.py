#!/usr/bin/env python3
"""Small raw-binary execution tests for the first MyEmulator 2.0 CPU."""

import os
import re
import socket
import struct
import subprocess
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
QEMU = Path(os.environ.get("QEMU_MYEMULATOR32",
                           ROOT / ".qemu-build/qemu-system-myemulator32"))


def r(rd, ra, rb, fn):
    return (rd << 21) | (ra << 16) | (rb << 11) | (fn << 6)


def i(rd, ra, sub, imm):
    return (1 << 26) | (rd << 21) | (ra << 16) | (sub << 12) | (imm & 0xfff)


def mem(op, rt, base, size, disp=0):
    return (op << 26) | (rt << 21) | (base << 16) | (size << 13) | (disp & 0x1fff)


def branch(cond, ra, rb, pc, target):
    disp = (target - (pc + 4)) // 4
    assert target % 4 == 0 and -4096 <= disp < 4096
    return (4 << 26) | (cond << 23) | (ra << 18) | (rb << 13) | (disp & 0x1fff)


def jump(op, pc, target):
    disp = (target - (pc + 4)) // 4
    assert target % 4 == 0 and -(1 << 25) <= disp < (1 << 25)
    return (op << 26) | (disp & 0x03ffffff)


def indirect(link, ra):
    return (7 << 26) | (link << 25) | (ra << 20)


def lui(rd, value):
    assert value & 0xfff == 0
    return (8 << 26) | (rd << 21) | ((value >> 12) << 1)


def sysins(sysop, reg=0, sysreg=0):
    return (9 << 26) | (sysop << 22) | (reg << 17) | (sysreg << 11)


HALT = sysins(3)
RFE = sysins(2)
MFSR = lambda rd, sr: sysins(0, rd, sr)
MTSR = lambda sr, rs: sysins(1, rs, sr)


def image(words, vectors=None, size=0x1000, initial_ssp=0x1000):
    data = bytearray(size)
    vectors = vectors or {}
    for address, value in ((0x400, initial_ssp), (0x404, 0x100)):
        struct.pack_into("<I", data, address, value)
    for address, value in vectors.items():
        struct.pack_into("<I", data, address, value)
    for address, value in words.items():
        struct.pack_into("<I", data, address, value)
    return data


class Monitor:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        deadline = time.time() + 5
        while True:
            try:
                self.sock.connect(path)
                break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(.01)
        self.sock.settimeout(5)
        self.buffer = b""
        self.recv_json()
        self.command_json("qmp_capabilities")

    def recv_json(self):
        while b"\n" not in self.buffer:
            self.buffer += self.sock.recv(4096)
        line, self.buffer = self.buffer.split(b"\n", 1)
        return __import__("json").loads(line)

    def command_json(self, name, arguments=None):
        import json
        request = {"execute": name, "id": name}
        if arguments is not None:
            request["arguments"] = arguments
        self.sock.sendall((json.dumps(request) + "\n").encode())
        while True:
            result = self.recv_json()
            if result.get("id") == name:
                return result

    def command(self, text):
        if text == "cont":
            self.command_json("cont")
            return ""
        result = self.command_json("human-monitor-command",
                                   {"command-line": text})
        return result.get("return", "")

    def close(self):
        self.sock.close()


def run_case(words, vectors=None, checks=None, stop_after=None,
             initial_ssp=0x1000):
    with tempfile.TemporaryDirectory(prefix="myemu32-") as directory:
        directory = Path(directory)
        image_path = directory / "program.bin"
        monitor_path = directory / "monitor.sock"
        image_path.write_bytes(image(words, vectors, initial_ssp=initial_ssp))
        proc = subprocess.Popen([
            str(QEMU), "-machine", "myemulator32", "-display", "none",
            "-kernel", str(image_path), "-S",
            "-qmp", f"unix:{monitor_path},server=on,wait=off",
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            try:
                monitor = Monitor(str(monitor_path))
            except Exception as exc:
                time.sleep(.05)
                stderr = proc.stderr.read().decode(errors="replace")
                stdout = proc.stdout.read().decode(errors="replace")
                raise AssertionError(f"QEMU failed to start (exit={proc.poll()}): "
                                     f"{exc}\nstdout={stdout}\nstderr={stderr}")
            monitor.command("cont")
            if stop_after is None:
                deadline = time.time() + 5
                output = ""
                while time.time() < deadline:
                    output = monitor.command("info registers")
                    if "halted=1" in output or "HALT" in output:
                        break
                    time.sleep(.01)
                else:
                    raise AssertionError("guest did not stop at HALT\n" + output)
            else:
                time.sleep(stop_after)
                monitor.command_json("stop")
                output = monitor.command("info registers")
            for pattern, expected in (checks or {}).items():
                match = re.search(pattern, output)
                if not match:
                    raise AssertionError(f"missing {pattern!r} in:\n{output}")
                if expected is not None and int(match.group(1), 16) != expected:
                    raise AssertionError(f"{pattern!r}: got {match.group(1)}")
            monitor.close()
        finally:
            proc.terminate()
            proc.wait(timeout=5)


def main():
    # Arithmetic, logical operations, shifts, word memory, BEQ/BNE, JAL/JR.
    words = {
        0x100: lui(1, 0x1000),
        0x104: i(1, 1, 0, 5),             # R1 = 0x1005
        0x108: i(2, 1, 1, 2),             # R2 = 0x1003
        0x10c: r(3, 1, 2, 0),             # ADD
        0x110: r(4, 3, 1, 2),             # SUB
        0x114: r(5, 4, 2, 11),            # AND
        0x118: r(6, 4, 2, 12),            # OR
        0x11c: r(7, 4, 2, 13),            # XOR
        0x120: r(8, 7, 0, 14),             # NOT
        0x124: i(9, 1, 5, 2),              # SLLI
        0x128: i(10, 9, 6, 1),             # SRLI
        0x12c: i(11, 10, 7, 1),            # SRAI
        0x130: mem(3, 11, 0, 2, 0x200),   # SW
        0x134: mem(2, 12, 0, 4, 0x200),   # LW
        0x138: branch(0, 11, 12, 0x138, 0x140),  # taken BEQ
        0x13c: i(13, 0, 0, 0x7f),         # skipped
        0x140: branch(1, 11, 12, 0x140, 0x148),  # not taken BNE
        0x144: jump(6, 0x144, 0x150),     # JAL
        0x148: HALT,
        0x14c: HALT,
        0x150: i(15, 0, 0, 0x2a),
        0x154: indirect(0, 14),            # JR LR
    }
    run_case(words, checks={
        r"R11: 0x([0-9a-f]+)": 0x1005,
        r"R12: 0x([0-9a-f]+)": 0x1005,
        r"R13: 0x([0-9a-f]+)": 0x1000,
        r"R15: 0x([0-9a-f]+)": 0x2a,
        r"PC: 0x([0-9a-f]+)": 0x14c,
    })

    # R0 is hardwired to zero, including when an instruction names it as Rd.
    run_case({0x100: i(0, 0, 0, 0x7f), 0x104: HALT},
             checks={r"R0: 0x([0-9a-f]+)": 0})

    # An odd JR target raises the instruction-alignment exception.  The
    # handler advances the saved faulting PC and returns with RFE.
    align_handler = {
        0x180: mem(2, 3, 13, 4, 0),
        0x184: i(3, 3, 0, 4),
        0x188: mem(3, 3, 13, 2, 0),
        0x18c: RFE,
    }
    run_case({0x100: i(1, 0, 0, 1),
              0x104: indirect(0, 1),
              0x108: HALT,
              **align_handler},
             vectors={8: 0x180},
             checks={r"R3: 0x([0-9a-f]+)": 0x108,
                     r"R13: 0x([0-9a-f]+)": 0x1000,
                     r"PC: 0x([0-9a-f]+)": 0x10c})

    # A misaligned LW follows the same retryable frame/RFE path.
    run_case({0x100: i(1, 0, 0, 1),
              0x104: mem(2, 2, 1, 4, 0),
              0x108: HALT,
              **align_handler},
             vectors={12: 0x180},
             checks={r"R3: 0x([0-9a-f]+)": 0x108,
                     r"PC: 0x([0-9a-f]+)": 0x10c})

    # Supervisor software sets both banked stacks, enters User mode, and
    # provokes a privilege fault.  The handler observes SSP, adjusts the
    # saved PC, and RFE returns to User mode with USP visible in R13.
    privilege_handler = {
        0x180: sysins(0, 4, 2),       # MFSR R4,SSP
        0x184: i(4, 4, 0, 16),         # recover pre-entry SSP
        0x188: mem(2, 3, 13, 4, 0),   # saved PC
        0x18c: i(3, 3, 0, 4),
        0x190: mem(3, 3, 13, 2, 0),
        0x194: RFE,
    }
    run_case({0x100: lui(1, 0x2000),
              0x104: MTSR(1, 1),
              0x108: MTSR(0, 0),
              0x10c: MFSR(2, 2),
              0x110: i(15, 0, 0, 0x55),
              0x114: branch(0, 0, 0, 0x114, 0x114),
              **privilege_handler},
             vectors={4: 0x180}, stop_after=0.05, initial_ssp=0x3000,
             checks={r"R4: 0x([0-9a-f]+)": 0x3000,
                     r"R13: 0x([0-9a-f]+)": 0x2000,
                     r"R15: 0x([0-9a-f]+)": 0x55,
                     r"SR: 0x([0-9a-f]+)": 0})
    print("myemulator32 CPU execution tests: PASS")


if __name__ == "__main__":
    main()
