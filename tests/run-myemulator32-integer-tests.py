#!/usr/bin/env python3
"""Focused tests for the non-MMU integer portion of MyEmulator 2.0."""

from pathlib import Path
import importlib.util

cpu_test_path = Path(__file__).resolve().parent / "run-myemulator32-cpu-tests.py"
spec = importlib.util.spec_from_file_location("myemu32_cpu_tests", cpu_test_path)
cpu_tests = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cpu_tests)
HALT = cpu_tests.HALT
MFSR = cpu_tests.MFSR
RFE = cpu_tests.RFE
branch = cpu_tests.branch
indirect = cpu_tests.indirect
jump = cpu_tests.jump
i = cpu_tests.i
lui = cpu_tests.lui
mem = cpu_tests.mem
r = cpu_tests.r
run_case = cpu_tests.run_case


def main():
    # Ordinary ADD/SUB boundaries and signed overflow flag behavior.
    run_case({
        0x100: lui(1, 0xfffff000),
        0x104: i(1, 1, 4, 0xfff),       # ffffffff
        0x108: i(2, 0, 0, 1),
        0x10c: r(3, 1, 2, 0),            # ffffffff + 1 = 0
        0x110: MFSR(4, 0),
        0x114: lui(1, 0x7ffff000),
        0x118: i(1, 1, 4, 0xfff),       # 7fffffff
        0x11c: r(5, 1, 2, 0),            # signed positive overflow
        0x120: MFSR(6, 0),
        0x124: lui(1, 0x80000000),
        0x128: r(7, 1, 2, 2),            # 80000000 - 1: negative overflow
        0x12c: MFSR(8, 0),
        0x130: HALT,
    }, checks={
        r"R3: 0x([0-9a-f]+)": 0,
        r"R4: 0x([0-9a-f]+)": 0x3d,
        r"R5: 0x([0-9a-f]+)": 0x80000000,
        r"R6: 0x([0-9a-f]+)": 0x3e,
        r"R7: 0x([0-9a-f]+)": 0x7fffffff,
        r"R8: 0x([0-9a-f]+)": 0x3e,
    })

    # Logical immediates use zero-extended 12-bit masks.
    run_case({
        0x100: i(1, 0, 3, 0xabc),       # ORI from zero
        0x104: i(2, 1, 2, 0x0f0),       # ANDI = 0x0b0
        0x108: i(3, 1, 4, 0xfff),       # XORI = 0x543
        0x10c: HALT,
    }, checks={
        r"R1: 0x([0-9a-f]+)": 0xabc,
        r"R2: 0x([0-9a-f]+)": 0x0b0,
        r"R3: 0x([0-9a-f]+)": 0x543,
    })

    # ADC uses carry-in and carry-out; SBC treats CF as an incoming borrow.
    run_case({
        0x100: lui(1, 0xfffff000),
        0x104: i(1, 1, 4, 0xfff),       # R1 = 0xffffffff
        0x108: i(0, 0, 1, 1),            # 0 - 1: CF=borrow=1
        0x10c: r(2, 1, 0, 1),            # ADC: ffffffff + 0 + 1
        0x110: MFSR(3, 0),
        0x114: r(4, 0, 0, 3),            # SBC: 0 - 0 - 1
        0x118: MFSR(5, 0),
        0x11c: HALT,
    }, checks={
        r"R2: 0x([0-9a-f]+)": 0,
        r"R3: 0x([0-9a-f]+)": 0x3d,
        r"R4: 0x([0-9a-f]+)": 0xffffffff,
        r"R5: 0x([0-9a-f]+)": 0x3d,
    })

    # A 64-bit subtraction uses the low-word borrow as the high-word SBC
    # input: 0x00000005_00000000 - 0x00000002_00000001 = 0x00000002_ffffffff.
    run_case({
        0x100: i(1, 0, 0, 0),
        0x104: i(2, 0, 0, 1),
        0x108: r(3, 1, 2, 2),            # low word: 0 - 1, borrow
        0x10c: i(4, 0, 0, 5),
        0x110: i(5, 0, 0, 2),
        0x114: r(6, 4, 5, 3),            # high word: 5 - 2 - borrow
        0x118: MFSR(7, 0),
        0x11c: HALT,
    }, checks={
        r"R3: 0x([0-9a-f]+)": 0xffffffff,
        r"R6: 0x([0-9a-f]+)": 2,
        r"R7: 0x([0-9a-f]+)": 0x3c,
    })

    # Signed and unsigned high products.
    run_case({
        0x100: lui(1, 0xfffff000),
        0x104: i(1, 1, 4, 0xfff),       # -1
        0x108: i(2, 0, 0, 2),
        0x10c: r(3, 1, 2, 4),            # MUL = fffffffe
        0x110: r(0, 1, 2, 4),            # discarded destination still executes
        0x114: r(4, 1, 2, 5),            # MULH = ffffffff
        0x118: r(5, 1, 2, 6),            # MULHU = 1
        0x11c: HALT,
    }, checks={
        r"R3: 0x([0-9a-f]+)": 0xfffffffe,
        r"R4: 0x([0-9a-f]+)": 0xffffffff,
        r"R5: 0x([0-9a-f]+)": 1,
    })

    # Signed truncation-toward-zero division/remainder and unsigned variants.
    run_case({
        0x100: lui(1, 0xfffff000),
        0x104: i(1, 1, 4, 0xffb),       # -5
        0x108: i(2, 0, 0, 2),
        0x10c: r(0, 1, 2, 7),            # discarded DIV destination
        0x110: r(3, 1, 2, 7),            # DIV = -2
        0x114: r(4, 1, 2, 9),            # REM = -1
        0x118: r(5, 1, 2, 8),            # DIVU = 7ffffffd
        0x11c: r(6, 1, 2, 10),           # REMU = 1
        0x120: HALT,
    }, checks={
        r"R3: 0x([0-9a-f]+)": 0xfffffffe,
        r"R4: 0x([0-9a-f]+)": 0xffffffff,
        r"R5: 0x([0-9a-f]+)": 0x7ffffffd,
        r"R6: 0x([0-9a-f]+)": 1,
    })

    # Register shifts, rotates, and canonical signed/unsigned comparisons.
    run_case({
        0x100: lui(1, 0x80000000),
        0x104: i(1, 1, 3, 1),            # 80000001
        0x108: i(2, 0, 0, 1),
        0x10c: r(3, 1, 2, 15),           # SLL
        0x110: r(4, 1, 2, 16),           # SRL
        0x114: r(5, 1, 2, 17),           # SRA
        0x118: r(6, 1, 2, 18),           # ROL
        0x11c: r(7, 1, 2, 19),           # ROR
        0x120: r(8, 1, 0, 22),           # SLT(negative, zero)
        0x124: r(9, 1, 0, 23),           # SGE
        0x128: r(10, 1, 0, 24),          # SLTU
        0x12c: r(11, 1, 0, 25),          # SGEU
        0x130: r(0, 1, 0, 22),            # discarded comparison destination
        0x134: r(12, 1, 1, 20),          # SEQ
        0x138: r(15, 1, 2, 21),          # SNE
        0x13c: HALT,
    }, checks={
        r"R3: 0x([0-9a-f]+)": 2,
        r"R4: 0x([0-9a-f]+)": 0x40000000,
        r"R5: 0x([0-9a-f]+)": 0xc0000000,
        r"R6: 0x([0-9a-f]+)": 3,
        r"R7: 0x([0-9a-f]+)": 0xc0000000,
        r"R8: 0x([0-9a-f]+)": 1,
        r"R9: 0x([0-9a-f]+)": 0,
        r"R10: 0x([0-9a-f]+)": 0,
        r"R11: 0x([0-9a-f]+)": 1,
        r"R12: 0x([0-9a-f]+)": 1,
        r"R15: 0x([0-9a-f]+)": 1,
    })

    # A raw instruction stream combining arithmetic, memory, comparisons,
    # branching, and a call/return sequence.
    run_case({
        0x100: i(1, 0, 0, 0x200),       # data address
        0x104: i(2, 0, 0, 7),
        0x108: i(3, 0, 0, 3),
        0x10c: r(4, 2, 3, 4),            # 7 * 3
        0x110: r(5, 4, 3, 7),            # 21 / 3
        0x114: mem(3, 5, 1, 2, 0),       # store word
        0x118: mem(3, 4, 1, 1, 4),       # store halfword
        0x11c: mem(2, 6, 1, 2, 4),       # signed halfword load
        0x120: r(7, 6, 5, 24),           # unsigned less-than: false
        0x124: branch(3, 6, 5, 0x124, 0x12c),  # BGEU taken
        0x128: i(8, 0, 0, 1),            # skipped
        0x12c: jump(6, 0x12c, 0x138),  # JAL
        0x130: HALT,
        0x138: i(9, 0, 0, 0x2a),
        0x13c: indirect(0, 14),          # JR LR
    }, checks={
        r"R4: 0x([0-9a-f]+)": 21,
        r"R5: 0x([0-9a-f]+)": 7,
        r"R6: 0x([0-9a-f]+)": 21,
        r"R7: 0x([0-9a-f]+)": 0,
        r"R8: 0x([0-9a-f]+)": 0,
        r"R9: 0x([0-9a-f]+)": 0x2a,
        r"PC: 0x([0-9a-f]+)": 0x134,
    })

    # Little-endian SB/SH/SW and signed/unsigned byte/halfword loads.
    run_case({
        0x100: i(1, 0, 0, 0x200),
        0x104: lui(2, 0xa1b2c000),
        0x108: i(2, 2, 3, 0x3d4),
        0x10c: mem(3, 2, 1, 0, 0),
        0x110: mem(3, 2, 1, 1, 2),
        0x114: mem(3, 2, 1, 2, 4),
        0x118: mem(2, 3, 1, 0, 0),
        0x11c: mem(2, 4, 1, 1, 0),
        0x120: mem(2, 5, 1, 2, 2),
        0x124: mem(2, 6, 1, 3, 2),
        0x128: mem(2, 7, 1, 4, 4),
        0x12c: mem(2, 0, 1, 4, 4),
        0x130: HALT,
    }, checks={
        r"R3: 0x([0-9a-f]+)": 0xffffffd4,
        r"R4: 0x([0-9a-f]+)": 0xd4,
        r"R5: 0x([0-9a-f]+)": 0xffffc3d4,
        r"R6: 0x([0-9a-f]+)": 0xc3d4,
        r"R7: 0x([0-9a-f]+)": 0xa1b2c3d4,
        r"R0: 0x([0-9a-f]+)": 0,
    })

    # Signed and unsigned branches distinguish ffffffff from zero.
    run_case({
        0x100: lui(1, 0xfffff000),
        0x104: i(1, 1, 4, 0xfff),
        0x108: branch(2, 1, 0, 0x108, 0x114),  # signed LT taken
        0x10c: i(3, 0, 0, 1),
        0x110: HALT,
        0x114: branch(4, 1, 0, 0x114, 0x120),  # unsigned LT not taken
        0x118: i(4, 0, 0, 2),
        0x11c: HALT,
        0x120: i(5, 0, 0, 3),
        0x124: HALT,
    }, checks={
        r"R3: 0x([0-9a-f]+)": 0,
        r"R4: 0x([0-9a-f]+)": 2,
        r"PC: 0x([0-9a-f]+)": 0x120,
    })

    # Divide by zero: destination remains unchanged and the saved PC is
    # advanced by the handler before RFE.
    div_handler = {
        0x180: mem(2, 3, 13, 2, 8),
        0x184: mem(2, 4, 13, 2, 0),
        0x188: i(4, 4, 0, 4),
        0x18c: mem(3, 4, 13, 2, 0),
        0x190: RFE,
    }
    run_case({
        0x100: i(1, 0, 0, 7),
        0x104: i(2, 0, 0, 0),
        0x108: i(6, 0, 0, 0x123),
        0x10c: r(6, 1, 2, 7),
        0x110: HALT,
        **div_handler,
    }, vectors={40: 0x180}, checks={
        r"R3: 0x([0-9a-f]+)": 10,
        r"R6: 0x([0-9a-f]+)": 0x123,
        r"PC: 0x([0-9a-f]+)": 0x114,
    })

    # Signed minimum divided by -1 raises arithmetic overflow and leaves the
    # destination unchanged.
    run_case({
        0x100: lui(1, 0x80000000),
        0x104: lui(2, 0xfffff000),
        0x108: i(2, 2, 4, 0xfff),
        0x10c: i(6, 0, 0, 0x321),
        0x110: r(6, 1, 2, 7),
        0x114: HALT,
        **div_handler,
    }, vectors={44: 0x180}, checks={
        r"R3: 0x([0-9a-f]+)": 11,
        r"R6: 0x([0-9a-f]+)": 0x321,
        r"PC: 0x([0-9a-f]+)": 0x118,
    })

    # Misaligned halfword load and store both take vector 3; the store does
    # not partially alter the word at 0x200.
    align_handler = {
        0x180: mem(2, 3, 13, 2, 8),
        0x184: mem(2, 4, 13, 2, 0),
        0x188: i(4, 4, 0, 4),
        0x18c: mem(3, 4, 13, 2, 0),
        0x190: RFE,
    }
    run_case({
        0x100: i(1, 0, 0, 0x200),
        0x104: i(2, 0, 0, 0x55),
        0x108: mem(3, 2, 1, 2, 0),
        0x10c: mem(2, 5, 1, 2, 1),
        0x110: mem(3, 2, 1, 1, 1),
        0x114: mem(2, 6, 1, 4, 0),
        0x118: HALT,
        **align_handler,
    }, vectors={12: 0x180}, checks={
        r"R3: 0x([0-9a-f]+)": 3,
        r"R5: 0x([0-9a-f]+)": 0,
        r"R6: 0x([0-9a-f]+)": 0x55,
    })

    print("myemulator32 integer/load-store tests: PASS")


if __name__ == "__main__":
    main()
