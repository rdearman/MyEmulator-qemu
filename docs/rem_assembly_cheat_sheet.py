#!/usr/bin/env python3
"""Generate the editable SVG and PDF-ready pages for the REM cheat sheet."""
from html import escape
from pathlib import Path
import subprocess

OUT = Path(__file__).resolve().parent
W, H = 1123, 794  # A4 landscape at 96 dpi
BG, PANEL, WHITE, MUTED, CYAN, CORAL, GRID = (
    "#182126", "#223038", "#edf4f5", "#aebfc2", "#54d6df", "#ff806f", "#3b5158"
)


class Page:
    def __init__(self, title, kicker):
        self.items = []
        self.baselines = []
        self.text(34, 42, kicker, 13, CORAL, "bold", "sans")
        self.text(34, 76, title, 30, WHITE, "bold", "sans")
        self.line(34, 94, W - 34, 94, CORAL, 2)

    def rect(self, x, y, w, h, fill=PANEL, stroke=GRID, r=8):
        self.items.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}"/>')

    def line(self, x1, y1, x2, y2, stroke=GRID, width=1):
        self.items.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{stroke}" stroke-width="{width}"/>')

    def text(self, x, y, value, size=12, fill=WHITE, weight="normal", family="sans", anchor="start"):
        self.baselines.append((x, y, value))
        self.items.append(
            f'<text x="{x}" y="{y}" font-family="{family}" font-size="{size}px" '
            f'font-weight="{weight}" fill="{fill}" text-anchor="{anchor}">{escape(value)}</text>'
        )

    def block(self, x, y, lines, size=11, leading=15, fill=WHITE, family="sans", weight="normal"):
        for i, line in enumerate(lines):
            self.text(x, y + i * leading, line, size, fill, weight, family)

    def heading(self, x, y, title, w):
        self.text(x, y, title.upper(), 12, CORAL, "bold")
        self.line(x, y + 8, x + w, y + 8, GRID)

    def code(self, x, y, lines, size=10, leading=14):
        self.block(x, y, lines, size, leading, CYAN, "monospace")

    def table(self, x, y, widths, rows, sizes=10, leading=16):
        yy = y
        for row_i, row in enumerate(rows):
            xx = x
            wrapped = []
            for col_i, cell in enumerate(row):
                chars = max(8, int((widths[col_i] - 10) / (sizes * (0.62 if col_i == 0 else 0.52))))
                lines = []
                for paragraph in str(cell).split("\n"):
                    words = paragraph.split()
                    current = ""
                    for word in words:
                        candidate = word if not current else current + " " + word
                        if current and len(candidate) > chars:
                            lines.append(current)
                            current = word
                        else:
                            current = candidate
                    lines.append(current)
                wrapped.append(lines)
            max_lines = max(map(len, wrapped))
            for col_i, cell in enumerate(row):
                lines = wrapped[col_i]
                self.text(xx + 5, yy + 12, lines[0], sizes,
                          CYAN if row_i == 0 else WHITE,
                          "bold" if row_i == 0 else "normal",
                          "monospace" if col_i == 0 else "sans")
                for li, line in enumerate(lines[1:]):
                    self.text(xx + 5, yy + 12 + li * leading, line, sizes, WHITE, "normal")
                xx += widths[col_i]
            self.line(x, yy + max_lines * leading + 2, x + sum(widths),
                      yy + max_lines * leading + 2)
            yy += max_lines * leading + 3

    def validate(self):
        for x, y, value in self.baselines:
            if not (20 <= x <= W - 20 and 20 <= y <= H - 12):
                raise ValueError(f"text outside page: {value!r} at {x},{y}")

    def svg(self):
        return (
            f'<svg xmlns="http://www.w3.org/2000/svg" width="297mm" height="210mm" '
            f'viewBox="0 0 {W} {H}"><title>REM Assembly Cheat Sheet</title>'
            f'<rect width="{W}" height="{H}" fill="{BG}"/>{"".join(self.items)}</svg>'
        )


def page1():
    p = Page("32-BIT ASSEMBLY CHEAT SHEET", "REM // CPU AND INSTRUCTION REFERENCE")
    # left column
    p.rect(34, 116, 300, 638)
    p.heading(50, 140, "Register map", 268)
    p.table(50, 151, [78, 180], [
        ("REG", "ARCHITECTURE / ABI"),
        ("r0 / zero", "hardwired zero"),
        ("r1–r4", "arguments; caller-saved"),
        ("r5–r12", "general; callee-saved"),
        ("r13 / sp", "banked stack pointer"),
        ("r14 / lr", "link / return register"),
        ("r15", "caller-saved temporary"),
    ], 10, 16)
    p.text(50, 302, "Special state", 11, CORAL, "bold")
    p.table(50, 312, [78, 180], [
        ("PC", "program counter"),
        ("SR", "status / privilege control"),
    ], 10, 16)
    p.text(50, 370, "ABI quick facts", 11, CORAL, "bold")
    p.block(50, 390, [
        "args: r1, r2, r3, r4; later args on stack",
        "return: r1 (32-bit), r1:r2 (64-bit, low word first)",
        "stack grows downward; 16-byte aligned at calls",
        "no red zone; nested calls preserve incoming lr",
    ], 10, 15, WHITE)
    p.heading(50, 466, "SR bits", 268)
    p.rect(50, 480, 268, 38, BG, CYAN, 3)
    for x, w, lab in [(50, 36, "CF 0"), (86, 36, "OF 1"), (122, 66, "IPL 2–4"),
                      (188, 32, "S 5"), (220, 98, "reserved 6–31")]:
        p.rect(x, 480, w, 38, BG, CYAN, 0)
        p.text(x + w / 2, 503, lab, 9, CYAN, "bold", "monospace", "middle")
    p.block(50, 542, [
        "CF: carry out; subtraction sets CF on unsigned borrow.",
        "OF: signed two's-complement overflow.",
        "ADC/SBC consume old CF as carry/borrow input.",
        "No N or Z flags: comparisons write canonical 0 or 1.",
    ], 10, 15, WHITE)
    p.heading(50, 626, "Memory model", 268)
    p.block(50, 646, [
        "32-bit words • little-endian bytes • natural alignment",
        "byte: any address   halfword: address % 2 == 0",
        "word: address % 4 == 0   effective = base + signed disp13",
    ], 9, 14, MUTED)
    p.code(50, 700, ["lbu r1, 1(r5)   lh r2, 2(r5)   lw r3, 4(r5)",
                     "sb  r3, 0(r5)   sh r3, 2(r5)   sw r3, 4(r5)"], 9, 14)
    # middle
    p.rect(350, 116, 420, 638)
    p.heading(366, 140, "Instruction quick reference", 388)
    p.text(366, 158, "All R-format operations:  rd, ra, rb", 9, MUTED)
    rows = [
        ("ARITHMETIC", "OPERATION"),
        ("add/adc rd,ra,rb", "ra + rb / + old CF"),
        ("sub/sbc rd,ra,rb", "ra − rb / − old CF"),
        ("mul/mulh/mulhu", "rd,ra,rb; low / signed high / unsigned high"),
        ("div/divu rd,ra,rb", "signed / unsigned quotient"),
        ("rem/remu rd,ra,rb", "signed / unsigned remainder"),
        ("and/or/xor rd,ra,rb", "bitwise"),
        ("not rd,ra,r0", "bitwise complement; rb must be r0"),
        ("sll/srl/sra rd,ra,rb", "logical left / right / arithmetic right"),
        ("rol/ror rd,ra,rb", "rotate left / right"),
        ("seq/sne rd,ra,rb", "equal / not equal → 0 or 1"),
        ("slt/sge rd,ra,rb", "signed < / ≥ → 0 or 1"),
        ("sltu/sgeu rd,ra,rb", "unsigned < / ≥ → 0 or 1"),
    ]
    p.table(366, 166, [148, 250], rows, 9, 14)
    p.text(366, 404, "IMMEDIATE  rd, ra, imm", 10, CORAL, "bold")
    p.block(366, 421, [
        "addi / subi     signed 12-bit arithmetic; flags update",
        "andi / ori / xori   zero-extended 12-bit mask",
        "slli / srli / srai   immediate shift count 0–31",
    ], 9, 14, WHITE, "monospace")
    p.text(366, 480, "MEMORY  rt, displacement(base)", 10, CORAL, "bold")
    p.block(366, 497, [
        "lb / lbu   signed / zero-extended byte",
        "lh / lhu   signed / zero-extended halfword",
        "lw         32-bit word       sb / sh / sw stores",
    ], 9, 14, WHITE, "monospace")
    p.text(366, 556, "BRANCH  ra, rb, target", 10, CORAL, "bold")
    p.block(366, 573, [
        "beq  bne  blt  bge  bltu  bgeu",
        "signed: blt/bge       unsigned: bltu/bgeu",
        "target = (PC + 4) + sign_extend(disp << 2)",
    ], 9, 14, WHITE, "monospace")
    p.text(366, 632, "CONTROL / SYSTEM", 10, CORAL, "bold")
    p.block(366, 649, [
        "j target       jal target       jr ra       jalr ra",
        "mfsr rd, sysreg   mtsr sysreg, rs   rfe",
        "syscall 0      tlbflush [rN]      cas rd, rs, 0(ra)",
        "break          halt",
    ], 9, 14, WHITE, "monospace")
    # right
    p.rect(786, 116, 303, 638)
    p.heading(802, 140, "Control transfer", 271)
    p.code(802, 164, [
        "beq r1, r2, equal",
        "jal function       # lr = PC + 4",
        "jr  lr             # return",
        "jalr r5            # indirect call",
        "j loop             # no link",
    ], 10, 16)
    p.line(822, 238, 1048, 238, CYAN, 2)
    p.line(822, 238, 822, 252, CYAN, 2)
    p.line(1048, 238, 1048, 252, CYAN, 2)
    p.text(935, 270, "target = (PC + 4) + sign_extend(disp × 4)", 9, CYAN,
           "bold", "monospace", "middle")
    p.block(802, 286, [
        "Branches and direct jumps use PC+4 as the base.",
        "Displacements are signed instruction units (×4).",
        "Targets must be 4-byte aligned.",
    ], 10, 15, WHITE)
    p.heading(802, 344, "Instruction formats", 271)
    p.table(802, 356, [74, 196], [
        ("FORMAT", "FIELDS"),
        ("R", "op rd ra rb fn"),
        ("I", "op rd ra sub imm12"),
        ("MEM", "op rt base size disp13"),
        ("B", "op cond ra rb disp13"),
        ("J", "op disp26"),
        ("IND", "op link ra"),
        ("U", "op rd imm20"),
        ("SYS", "op sysop reg sysreg"),
        ("CAS", "op rd rs ra"),
    ], 8, 13)
    p.heading(802, 558, "Atomic CAS", 271)
    p.code(802, 572, [
        "retry:",
        "  lw   r1, 0(r5)       # expected",
        "  add  r3, r1, r0",
        "  addi r2, r1, 1       # desired",
        "  cas  r3, r2, 0(r5)   # r3 = old",
        "  bne  r3, r1, retry",
    ], 9, 14)
    p.block(802, 674, [
        "CAS compares original rd, stores rs on match,",
        "then always writes observed old value to rd.",
    ], 9, 14, MUTED)
    return p


def page2():
    p = Page("ASSEMBLY PROGRAMMING QUICK REFERENCE", "REM // PRACTICAL PROGRAMMING")
    p.rect(34, 116, 340, 638)
    p.heading(50, 140, "ABI and stack frame", 308)
    p.table(50, 151, [92, 216], [
        ("REGISTERS", "ROLE"),
        ("r1–r4", "args; caller-saved"),
        ("r5–r12", "callee-saved"),
        ("r13 / sp", "downward stack; callee-managed"),
        ("r14 / lr", "return link; save before nested call"),
        ("r15", "caller-saved temporary"),
    ], 9, 15)
    p.text(50, 286, "Nested-call frame (stack grows ↓)", 10, CORAL, "bold")
    p.rect(70, 300, 230, 112, BG, CYAN, 3)
    p.text(185, 320, "higher addresses", 9, MUTED, "normal", "sans", "middle")
    p.line(70, 330, 300, 330, CYAN)
    p.text(185, 350, "saved r5 / locals", 10, WHITE, "normal", "monospace", "middle")
    p.line(70, 360, 300, 360, CYAN)
    p.text(185, 380, "saved lr", 10, WHITE, "normal", "monospace", "middle")
    p.line(70, 390, 300, 390, CYAN)
    p.text(185, 407, "sp →", 10, CYAN, "bold", "monospace", "middle")
    p.code(50, 444, [
        "caller:",
        "  addi sp, sp, -16",
        "  sw   r5, 0(sp)",
        "  sw   lr, 4(sp)",
        "  jal  callee",
        "  lw   lr, 4(sp)",
        "  lw   r5, 0(sp)",
        "  addi sp, sp, 16",
        "  ret",
    ], 10, 15)
    p.block(50, 620, [
        "Call boundaries are 16-byte aligned; no red zone.",
        "r1 returns 32-bit values; r1:r2 returns 64-bit values.",
    ], 9, 14, MUTED)
    p.rect(390, 116, 340, 638)
    p.heading(406, 140, "GAS syntax and patterns", 308)
    p.code(406, 158, [
        ".section .text      .section .rodata",
        ".section .data      .section .bss",
        ".global main        .type main,@function",
        ".size main, .-main",
        ".byte 0x7f          .short 0x1234",
        ".word 0x12345678    .space 64",
        ".ascii \"REM\\n\"      .asciz \"REM\"",
        ".align 2            label:",
        "1:  j 1b             # numeric local label",
        "# comment            .equ N, 10",
    ], 9, 14)
    p.text(406, 342, "Full symbol address (HI20 / LO12)", 10, CORAL, "bold")
    p.code(406, 360, [
        "lui r5, %hi(symbol) # R_MYEMU_HI20",
        "ori r5, r5, %lo(symbol) # R_MYEMU_LO12",
    ], 10, 15)
    p.text(406, 412, "Supported pseudo-instructions", 10, CORAL, "bold")
    p.code(406, 430, [
        "li rd, constant      # addi or lui/ori pair",
        "call symbol          # jal symbol",
        "ret                   # jr r14",
    ], 10, 15)
    p.text(406, 484, "Common patterns", 10, CORAL, "bold")
    p.code(406, 502, [
        "li   r1, 0x12345678",
        "lw   r2, 0(r5)       # read",
        "sw   r2, 4(r5)       # write",
        "loop: addi r1, r1, -1",
        "      bne  r1, r0, loop",
        "add r1, r1, r3     # 64-bit low",
        "adc r2, r2, r4     # 64-bit high",
        "sub r1, r1, r3     # 64-bit low",
        "sbc r2, r2, r4     # 64-bit high",
    ], 9, 14)
    p.rect(746, 116, 343, 638)
    p.heading(762, 140, "Linux syscall ABI", 311)
    p.code(762, 158, [
        "r1 = syscall number",
        "r2–r7 = six arguments",
        "syscall 0",
        "r1 = signed return value",
        "r1 < 0  →  -errno",
    ], 10, 15)
    p.table(762, 220, [62, 135, 114], [
        ("NR", "CALL", "PURPOSE"),
        ("56", "openat", "open path"),
        ("57", "close", "close fd"),
        ("61", "getdents64", "read directory"),
        ("62", "lseek", "move offset"),
        ("63", "read", "read bytes"),
        ("64", "write", "write bytes"),
        ("93/94", "exit/group", "terminate"),
        ("160", "uname", "kernel identity"),
        ("214/215", "brk/munmap", "memory"),
        ("222", "mmap", "map memory"),
        ("403", "clock_gettime", "time"),
    ], 8, 11)
    p.text(762, 410, "Write a string", 10, CORAL, "bold")
    p.code(762, 428, [
        ".section .rodata",
        "msg: .ascii \"Hello from REM\\n\"",
        "msg_end:",
        ".section .text",
        ".global _start",
        "_start:",
        "lui  r3, %hi(msg)",
        "ori  r3, r3, %lo(msg)",
        "li   r1, 64       # write",
        "li   r2, 1        # stdout",
        "li   r4, msg_end-msg",
        "syscall 0",
        "li   r1, 94       # exit_group",
        "li   r2, 0",
        "syscall 0",
    ], 9, 14)
    p.text(762, 646, "Build / inspect / run", 10, CORAL, "bold")
    p.code(762, 664, [
        "myemulator2-elf-as -o x.o x.S",
        "myemulator2-elf-ld -T toolchain/userspace/myemulator2-user.ld -o x x.o",
        "myemulator2-elf-objdump -dr x",
        "myemulator2-elf-readelf -h -s -r x",
        "qemu-system-myemulator32 -M myemulator32 \\",
        "  -kernel vmlinux -nographic",
    ], 8, 12)
    return p


def main():
    pages = [page1(), page2()]
    for page in pages:
        page.validate()
    paths = []
    for i, page in enumerate(pages, 1):
        path = OUT / f"REM_ASSEMBLY_CHEAT_SHEET_PAGE{i}.svg"
        path.write_text(page.svg(), encoding="utf-8")
        paths.append(path)
    pdfs = []
    for i, path in enumerate(paths, 1):
        one = OUT / f".REM_ASSEMBLY_CHEAT_SHEET_PAGE{i}.pdf"
        subprocess.run(["inkscape", str(path), "--export-type=pdf",
                        f"--export-filename={one}"], check=True)
        pdfs.append(one)
    pdf = OUT / "REM_ASSEMBLY_CHEAT_SHEET.pdf"
    subprocess.run(["pdfunite", *map(str, pdfs), str(pdf)], check=True)
    for one in pdfs:
        one.unlink()
    print("\n".join(map(str, [*paths, pdf])))


if __name__ == "__main__":
    main()
