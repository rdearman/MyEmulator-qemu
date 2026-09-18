#!/usr/bin/env python3
"""System, interrupt, and MMU tests for MyEmulator 2.0."""

import importlib.util
import json
import re
import struct
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QEMU = Path(__import__("os").environ.get(
    "QEMU_MYEMULATOR32", ROOT / ".qemu-build/qemu-system-myemulator32"))

spec = importlib.util.spec_from_file_location(
    "cpu32", Path(__file__).with_name("run-myemulator32-cpu-tests.py"))
cpu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cpu)

HALT = cpu.HALT
RFE = cpu.RFE
MFSR = cpu.MFSR
MTSR = cpu.MTSR
branch = cpu.branch
i = cpu.i
lui = cpu.lui
mem = cpu.mem
r = cpu.r
sysins = cpu.sysins


def syscall(number):
    return (10 << 26) | (number & 0x03ffffff)


def tlbflush(page, ra=0):
    return (11 << 26) | (page << 25) | (ra << 20)


def make_image(words, vectors=None, initial_ssp=0x8000, size=0x10000,
               data=None):
    image = bytearray(size)
    struct.pack_into("<I", image, 0x400, initial_ssp)
    struct.pack_into("<I", image, 0x404, 0x100)
    for address, value in (vectors or {}).items():
        struct.pack_into("<I", image, address, value)
    for address, value in (data or {}).items():
        struct.pack_into("<I", image, address, value)
    for address, value in words.items():
        struct.pack_into("<I", image, address, value)
    return image


class Monitor(cpu.Monitor):
    def command_json(self, name, arguments=None):
        return super().command_json(name, arguments)


def start(words, vectors=None, **kwargs):
    directory = tempfile.TemporaryDirectory(prefix="myemu32-system-")
    path = Path(directory.name)
    image_path = path / "program.bin"
    qmp_path = path / "monitor.sock"
    image_path.write_bytes(make_image(words, vectors, **kwargs))
    proc = subprocess.Popen([
        str(QEMU), "-machine", "myemulator32", "-display", "none",
        "-kernel", str(image_path), "-S",
        "-qmp", f"unix:{qmp_path},server=on,wait=off",
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        monitor = Monitor(str(qmp_path))
    except Exception:
        stderr = proc.stderr.read().decode(errors="replace")
        proc.kill()
        raise AssertionError(f"QEMU failed to start: {stderr}")
    return directory, proc, monitor


def wait_halt(monitor):
    deadline = time.time() + 5
    output = ""
    while time.time() < deadline:
        output = monitor.command("info registers")
        if "halted=1" in output:
            return output
        time.sleep(.01)
    raise AssertionError("guest did not halt\n" + output)


def run(words, vectors=None, qmp=None, checks=(), **kwargs):
    directory, proc, monitor = start(words, vectors, **kwargs)
    try:
        if qmp:
            qmp(monitor)
        else:
            monitor.command("cont")
        output = wait_halt(monitor)
        for pattern, expected in checks:
            match = re.search(pattern, output)
            if not match:
                raise AssertionError(f"missing {pattern!r} in:\n{output}")
            if expected is not None and int(match.group(1), 16) != expected:
                raise AssertionError(f"{pattern!r}: got {match.group(1)}")
    finally:
        monitor.close()
        proc.terminate()
        proc.wait(timeout=5)
        directory.cleanup()


def main():
    # SYSCALL records its vector/cause and immediate in the fixed frame.
    syscall_handler = {
        0x200: mem(2, 2, 13, 4, 8),
        0x204: mem(2, 3, 13, 4, 12),
        0x208: HALT,
    }
    run({0x100: syscall(0x12345), **syscall_handler},
        vectors={12 * 4: 0x200}, checks=[
            (r"R2: 0x([0-9a-f]+)", 12),
            (r"R3: 0x([0-9a-f]+)", 0x12345),
            (r"PC: 0x([0-9a-f]+)", 0x20c),
        ])

    # VBR is relocatable on a 1 KiB boundary and the relocated table is used
    # for subsequent exception dispatch.
    run({0x100: i(1, 0, 3, 0x800), 0x104: MTSR(3, 1),
         0x108: syscall(7), 0x900: HALT},
        vectors={0x800 + 12 * 4: 0x900},
        checks=[(r"VBR: 0x([0-9a-f]+)", 0x800),
                (r"PC: 0x([0-9a-f]+)", 0x904)])

    # A User-mode privileged operation enters through vector 1 and uses SSP,
    # while the handler can inspect the pre-exception SR.
    privilege_handler = {
        0x200: mem(2, 2, 13, 4, 0),
        0x204: mem(2, 3, 13, 4, 4),
        0x208: HALT,
    }
    run({0x100: i(1, 0, 0, 0),
         0x104: MTSR(0, 1),
         0x108: MFSR(4, 2), **privilege_handler},
        vectors={4: 0x200}, checks=[
            (r"R2: 0x([0-9a-f]+)", 0x108),
            (r"R3: 0x([0-9a-f]+)", 0),
            (r"R13: 0x([0-9a-f]+)", 0x7ff0),
        ])

    # IRQ3 is accepted after software lowers IPL to zero.  The QMP debug
    # input is deliberately level-sensitive and is released after entry.
    irq_handler = {0x200: i(2, 0, 0, 0x33), 0x204: HALT}
    def trigger_irq(monitor):
        monitor.command("cont")
        wait_halt(monitor)
        monitor.command_json("myemulator32-debug",
                             {"op": "irq", "level": 3, "asserted": True})
        monitor.command("cont")
        time.sleep(.02)
        monitor.command_json("myemulator32-debug",
                             {"op": "irq", "level": 3, "asserted": False})

    run({0x100: i(1, 0, 0, 0x20), 0x104: MTSR(0, 1),
         0x108: HALT, **irq_handler}, vectors={18 * 4: 0x200},
        qmp=trigger_irq, checks=[(r"R2: 0x([0-9a-f]+)", 0x33),
                                 (r"PC: 0x([0-9a-f]+)", 0x208)])

    # NMI is accepted even at IPL=7 and is suppressed while its handler is
    # active.  The handler stops before RFE so the active state is observable.
    nmi_handler = {0x240: i(2, 0, 0, 0x44), 0x244: HALT}
    def trigger_nmi(monitor):
        monitor.command("cont")
        wait_halt(monitor)
        monitor.command_json("myemulator32-debug",
                             {"op": "nmi", "asserted": True})
        monitor.command("cont")
        time.sleep(.02)
        monitor.command_json("myemulator32-debug",
                             {"op": "nmi", "asserted": False})

    run({0x100: HALT, **nmi_handler}, vectors={14 * 4: 0x240},
        qmp=trigger_nmi, checks=[(r"R2: 0x([0-9a-f]+)", 0x44),
                                 (r"PC: 0x([0-9a-f]+)", 0x248)])

    # Enable paging, access a mapped data page, update its PTE through a
    # mapped page-table alias, and prove a page-specific TLB flush changes the
    # observed physical frame.
    flags = 0x1f  # P|U|R|W|X
    page_tables = {
        0x1000: 0x2000 | flags,
        0x2000 + 0 * 4: 0x0000 | flags,
        0x2000 + 2 * 4: 0x3000 | flags,
        0x2000 + 4 * 4: 0x2000 | flags,
        0x2000 + 8 * 4: 0x8000 | flags,
        0x3000: 0x11111111,
        0x5000: 0x22222222,
    }
    mmu_words = {
        0x100: lui(1, 0x1000),
        0x104: MTSR(4, 1),
        0x108: i(1, 0, 0, 1),
        0x10c: MTSR(5, 1),
        0x110: lui(2, 0x2000),
        0x114: mem(2, 3, 2, 4, 0),
        0x118: lui(4, 0x4000),
        0x11c: lui(5, 0x5000),
        0x120: i(5, 5, 0, flags),
        0x124: mem(3, 5, 4, 2, 8),
        0x128: mem(2, 6, 2, 4, 0),
        0x12c: lui(7, 0x2000),
        0x130: tlbflush(1, 7),
        0x134: mem(2, 8, 2, 4, 0),
        0x138: mem(2, 10, 4, 4, 0),
        0x13c: mem(2, 11, 4, 4, 8),
        0x140: HALT,
    }
    run(mmu_words, data=page_tables, checks=[
        (r"R3: 0x([0-9a-f]+)", 0x11111111),
        (r"R6: 0x([0-9a-f]+)", 0x11111111),
        (r"R8: 0x([0-9a-f]+)", 0x22222222),
        (r"R10: 0x([0-9a-f]+)", 0x3f),
        (r"R11: 0x([0-9a-f]+)", 0x503f),
        (r"MMCR: 0x([0-9a-f]+)", 1),
    ])

    # A missing second-level entry produces the distinct load-not-present
    # vector and reports the original virtual address in Info.
    fault_tables = {
        0x1000: 0x2000 | flags,
        0x2000: flags,
        0x2000 + 7 * 4: 0x7000 | flags,
        0x2000 + 8 * 4: 0x8000 | flags,
    }
    fault_handler = {
        0x280: mem(2, 2, 13, 4, 8),
        0x284: mem(2, 3, 13, 4, 12),
        0x288: mem(2, 4, 13, 4, 0),
        0x28c: HALT,
    }
    run({0x100: lui(1, 0x1000), 0x104: MTSR(4, 1),
         0x108: i(1, 0, 0, 1), 0x10c: MTSR(5, 1),
         0x110: lui(2, 0x6000), 0x114: mem(2, 4, 2, 4, 0),
         **fault_handler}, vectors={6 * 4: 0x280}, data=fault_tables,
        checks=[(r"R2: 0x([0-9a-f]+)", 6),
                (r"R3: 0x([0-9a-f]+)", 0x6000),
                (r"R4: 0x([0-9a-f]+)", 0x114)])

    # TIME is readable without privilege and a Supervisor-programmed
    # comparator raises ordinary IRQ1 once virtual time reaches it.
    timer_handler = {0x240: i(2, 0, 0, 0x77),
                     0x244: lui(3, 0xfffff000),
                     0x248: i(3, 3, 4, 0xfff),
                     0x24c: MTSR(8, 3),
                     0x250: MTSR(9, 0),
                     0x254: HALT}
    run({0x100: i(1, 0, 0, 0),       # comparator at current TIME: immediate
         0x104: MTSR(8, 1),
         0x108: MTSR(9, 0),          # commit high half
         0x10c: i(4, 0, 0, 0x20),
         0x110: MTSR(0, 4),          # Supervisor, IPL=0
         0x114: branch(0, 0, 0, 0x114, 0x114),
         **timer_handler}, vectors={16 * 4: 0x240}, checks=[
             (r"R2: 0x([0-9a-f]+)", 0x77),
             (r"SR: 0x([0-9a-f]+)", 0x24)])

    print("myemulator32 system/IRQ/NMI/MMU tests: PASS")


if __name__ == "__main__":
    main()
