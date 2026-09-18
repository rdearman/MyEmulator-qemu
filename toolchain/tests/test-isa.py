#!/usr/bin/env python3
"""Assembler coverage for every real MyEmulator2 instruction mnemonic."""

from pathlib import Path
import subprocess
import sys
import tempfile


REAL = """
.text
start:
 add r1,r2,r3
 adc r1,r2,r3
 sub r1,r2,r3
 sbc r1,r2,r3
 mul r1,r2,r3
 mulh r1,r2,r3
 mulhu r1,r2,r3
 div r1,r2,r3
 divu r1,r2,r3
 rem r1,r2,r3
 remu r1,r2,r3
 and r1,r2,r3
 or r1,r2,r3
 xor r1,r2,r3
 not r1,r2,r3
 sll r1,r2,r3
 srl r1,r2,r3
 sra r1,r2,r3
 rol r1,r2,r3
 ror r1,r2,r3
 seq r1,r2,r3
 sne r1,r2,r3
 slt r1,r2,r3
 sge r1,r2,r3
 sltu r1,r2,r3
 sgeu r1,r2,r3
 addi r1,r2,-2048
 subi r1,r2,2047
 andi r1,r2,0xfff
 ori r1,r2,0xfff
 xori r1,r2,0
 slli r1,r2,31
 srli r1,r2,0
 srai r1,r2,31
 lb r1,0(r2)
 lbu r1,1(r2)
 lh r1,2(r2)
 lhu r1,4(r2)
 lw r1,8(r2)
 sb r1,0(r2)
 sh r1,2(r2)
 sw r1,4(r2)
 beq r1,r2,start
 bne r1,r2,start
 blt r1,r2,start
 bge r1,r2,start
 bltu r1,r2,start
 bgeu r1,r2,start
 j start
 jal start
 jr r5
 jalr r6
 lui r7,0x12345
 mfsr r1,sr
 mfsr r1,usp
 mfsr r1,ssp
 mfsr r1,vbr
 mfsr r1,ptbr
 mfsr r1,mmcr
 mfsr r1,time
 mfsr r1,time_hi
 mfsr r1,timecmp
 mfsr r1,timecmp_hi
 mfsr r1,tp
 mfsr r1,dfsp
 mtsr sr,r1
 mtsr tp,r1
 rfe
 halt
 break
 syscall 12
 tlbflush
 tlbflush r5
 cas r1,r2,0(r3)
 ret
 call start
 li r8,0x12345678
"""


def run(command, *, expect=0):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    if (result.returncode == 0) != (expect == 0):
        raise AssertionError(f"{command!r}: {result.stderr}")
    return result


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: test-isa.py BINUTILS-BIN")
    bindir = Path(sys.argv[1]).resolve()
    assembler = bindir / "myemulator2-elf-as"
    objdump = bindir / "myemulator2-elf-objdump"
    with tempfile.TemporaryDirectory(prefix="myemu2-isa-") as name:
        directory = Path(name)
        source = directory / "all.s"
        obj = directory / "all.o"
        source.write_text(REAL)
        run([str(assembler), "-o", str(obj), str(source)])
        listing = run([str(objdump), "-dr", str(obj)]).stdout.lower()
        for mnemonic in ("add", "adc", "sub", "sbc", "mul", "mulh", "mulhu",
                         "div", "divu", "rem", "remu", "and", "or", "xor",
                         "not", "sll", "srl", "sra", "rol", "ror", "seq",
                         "sne", "slt", "sge", "sltu", "sgeu", "lb", "lbu",
                         "lh", "lhu", "lw", "sb", "sh", "sw", "beq", "bne",
                         "blt", "bge", "bltu", "bgeu", "j", "jal", "jr",
                         "jalr", "lui", "mfsr", "mtsr", "rfe", "halt", "break",
                         "syscall", "tlbflush", "cas"):
            if mnemonic not in listing:
                raise AssertionError(f"missing {mnemonic} in disassembly")
        bad = directory / "bad.s"
        bad.write_text(".text\n add r16,r1,r2\n")
        run([str(assembler), "-o", str(directory / "bad.o"), str(bad)], expect=1)
    print("ISA assembler/disassembler coverage: PASS")


if __name__ == "__main__":
    main()
