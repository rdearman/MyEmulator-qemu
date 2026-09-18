# MyEmulator 2.0 Architecture Specification

**Status: DESIGN / NOT IMPLEMENTED**

**Specification revision: 2.0-design-1**

MyEmulator 2.0 is a new 32-bit architecture. It has no binary, ISA,
execution-mode, register, opcode, peripheral, firmware, or source-compatibility
requirement with MyEmulator 1.0. The completed 1.0 implementation remains in
this repository as a tagged reference and is not modified by this
specification.

The machine-readable mechanical encoding manifest is
[`myemulator2-encoding.json`](myemulator2-encoding.json). It is deliberately
validated independently by `tools/test_myemulator2_spec.py`; it is data for
the future implementation, not an executable CPU description.

## 1. Architectural model

MyEmulator 2.0 is a little-endian, byte-addressed CPU with a 32-bit virtual
address and 32-bit physical address space. Addresses and integer operations
are modulo 2^32 unless an operation explicitly raises an exception. Integers
are two's-complement.

Instructions are fixed-width 32-bit words stored little-endian and must be
4-byte aligned. Normal sequential execution advances `PC` by 4. There are no
legacy execution modes.

There are sixteen encoded GPRs:

| Register | Meaning |
| --- | --- |
| `R0` / `ZERO` | hardwired zero |
| `R1`-`R12` | general purpose |
| `R13` / `SP` | banked stack pointer |
| `R14` / `LR` | link register |
| `R15` | general purpose |

`PC` is a separate 32-bit architectural register. `SR` is a separate
32-bit status register. R0 reads as zero and writes to it are discarded
without causing an exception.

### Privilege and banked SP

There are exactly two modes, selected by `SR.S`: User (`S=0`) and Supervisor
(`S=1`). R13 is a view of one of two physical registers:

```text
User mode:       R13/SP -> USP
Supervisor mode: R13/SP -> SSP
```

Supervisor software can access both `USP` and `SSP` through system-register
access. User software cannot access `SSP` directly. Changing `SR.S` changes
the R13 view automatically.

### SR layout

| Bits | Name | Meaning |
| --- | --- | --- |
| 0 | CF | carry on addition; borrow on subtraction |
| 1 | OF | signed two's-complement overflow |
| 2-4 | IPL | unsigned interrupt priority level, 0-7 |
| 5 | S | Supervisor mode when 1 |
| 6-31 | reserved | reads as zero; writes have no effect |

There is no Z flag, N flag, or separate interrupt-enable flag. IPL 7 masks
all ordinary IRQ1-IRQ7. Logical, multiply, divide, comparison, load/store,
control-transfer, and system instructions leave CF/OF unchanged unless a
specific exception or arithmetic operation below says otherwise.

## 2. Instruction word formats

All fields are numbered with bit 31 as the most significant bit. `op` is the
6-bit primary opcode in bits 31-26. Register fields are five bits wide so the
encoding remains regular; values 0-15 name architectural registers and
values 16-31 are reserved.

The native decoder must reject register-field values 16-31 as illegal
instructions. Encodings marked reserved below must also be rejected as
illegal instructions.

### R format

```text
31          26 25       21 20       16 15       11 10       6 5       0
+-------------+-----------+-----------+-----------+-----------+---------+
| op = 0x00   | rd        | ra        | rb        | fn        | 0       |
+-------------+-----------+-----------+-----------+-----------+---------+
```

### ALU-immediate format

```text
31          26 25       21 20       16 15   12 11            0
+-------------+-----------+-----------+--------+---------------+
| op = 0x01   | rd        | ra        | sub    | imm12         |
+-------------+-----------+-----------+--------+---------------+
```

`ADDI` and `SUBI` sign-extend `imm12`. `ANDI`, `ORI`, and `XORI` zero-extend
it. Immediate shift forms use only `imm12[4:0]`; `imm12[11:5]` must be zero.

### Memory format

```text
31          26 25       21 20       16 15 13 12             0
+-------------+-----------+-----------+-----+----------------+
| op           | rt        | base      | size| disp13         |
+-------------+-----------+-----------+-----+----------------+
```

`disp13` is signed and added to the 32-bit base register. For loads `rt` is
the destination; for stores it is the source. `size` is operation-specific.

### Branch format

```text
31          26 25   23 22      18 17      13 12             0
+-------------+-------+----------+----------+----------------+
| op = 0x04   | cond  | ra       | rb       | disp13         |
+-------------+-------+----------+----------+----------------+
```

### Direct jump format

```text
31          26 25                                      0
+-------------+-----------------------------------------+
| op           | disp26                                  |
+-------------+-----------------------------------------+
```

### Indirect, upper-immediate, and system formats

```text
IND:     op[31:26], link[25], ra[24:20], reserved[19:0]=0
U:       op[31:26], rd[25:21], imm20[20:1], reserved[0]=0
SYS:     op[31:26], sysop[25:22], reg[21:17], sysreg[16:11], reserved[10:0]=0
SYSCALL: op[31:26], immediate[25:0]
TLB:     op[31:26], page[25], ra[24:20], reserved[19:0]=0
```

The exact fields and numeric assignments are also present in the JSON
manifest.

## 3. Opcode allocation

Only primary opcodes 0x00 through 0x0B are assigned. 0x0C-0x3F are reserved
for future architectural expansion and must currently cause illegal
instruction exceptions.

### Primary opcodes

| Opcode | Family |
| ---: | --- |
| `0x00` | R-format ALU/comparison, selected by `fn` |
| `0x01` | immediate ALU/shift, selected by `sub` |
| `0x02` | loads, selected by `size` |
| `0x03` | stores, selected by `size` |
| `0x04` | conditional branches, selected by `cond` |
| `0x05` | `J` |
| `0x06` | `JAL` |
| `0x07` | `JR`/`JALR`, selected by `link` |
| `0x08` | `LUI` |
| `0x09` | system operations, selected by `sysop` |
| `0x0A` | `SYSCALL` |
| `0x0B` | `TLBFLUSH`, selected by `page` |

### R-format functions

For every R-format operation, operands are read before `rd` is written. The
low six bits of the word must be zero.

| `fn` | Instruction | `fn` | Instruction |
| ---: | --- | ---: | --- |
| 0 | `ADD` | 13 | `XOR` |
| 1 | `ADC` | 14 | `NOT` |
| 2 | `SUB` | 15 | `SLL` |
| 3 | `SBC` | 16 | `SRL` |
| 4 | `MUL` | 17 | `SRA` |
| 5 | `MULH` | 18 | `ROL` |
| 6 | `MULHU` | 19 | `ROR` |
| 7 | `DIV` | 20 | `SEQ` |
| 8 | `DIVU` | 21 | `SNE` |
| 9 | `REM` | 22 | `SLT` |
| 10 | `REMU` | 23 | `SGE` |
| 11 | `AND` | 24 | `SLTU` |
| 12 | `OR` | 25 | `SGEU` |

Functions 26-31 are reserved. `NOT Rd,Ra` requires `rb=R0`; other unused
operand encodings are illegal.

### Immediate suboperations

| `sub` | Instruction | Immediate interpretation |
| ---: | --- | --- |
| 0 | `ADDI` | signed 12-bit |
| 1 | `SUBI` | signed 12-bit |
| 2 | `ANDI` | zero-extended 12-bit |
| 3 | `ORI` | zero-extended 12-bit |
| 4 | `XORI` | zero-extended 12-bit |
| 5 | `SLLI` | count 0-31; upper 7 bits must be zero |
| 6 | `SRLI` | count 0-31; upper 7 bits must be zero |
| 7 | `SRAI` | count 0-31; upper 7 bits must be zero |

Suboperations 8-15 are reserved.

### Loads and stores

Load `size` values are `LB=0`, `LBU=1`, `LH=2`, `LHU=3`, `LW=4`; values 5-7
are reserved. Store `size` values are `SB=0`, `SH=1`, `SW=2`; values 3-7
are reserved.

The signed 13-bit displacement is added to `base`. No addressing instruction
modifies the base register. `LB` sign-extends 8 bits, `LBU` zero-extends 8,
`LH` sign-extends 16, `LHU` zero-extends 16, and `LW` loads all 32 bits.
Stores use the low 8 bits for `SB`, low 16 for `SH`, and all 32 for `SW`.

### Branches and direct jumps

Branch conditions are `BEQ=0`, `BNE=1`, `BLT=2`, `BGE=3`, `BLTU=4`, and
`BGEU=5`; 6-7 are reserved. Signed conditions interpret operands as
two's-complement; unsigned conditions do not.

For a branch or direct jump, let `base = PC + 4` where PC is the address of
the instruction. The target is:

```text
target = base + (sign_extend(displacement) << 2)
```

The displacement is in 4-byte instruction units. A branch displacement is
13 bits; a `J` or `JAL` displacement is 26 bits. A not-taken conditional
branch sets PC to `base`. Direct encodings always produce 4-byte-aligned
targets.

`J` changes only PC. `JAL` writes `LR=R14=PC+4` and then changes PC.
`JR Ra` changes PC to `Ra`; `JALR Ra` first validates the target, then writes
`LR=PC+4` and changes PC. Indirect targets with bits 1:0 nonzero cause an
instruction-alignment exception and do not update PC or LR. `RET` is an
assembler alias for `JR R14`; `CALL target` is an alias for `JAL target`.

### LUI and assembler constants

`LUI Rd,imm20` writes `imm20 << 12` to Rd. Bit 0 of the encoding is zero and
the immediate is held in bits 20:1. `LI Rd,constant` is a pseudo-instruction,
not an opcode. A conforming assembler may use `ADDI Rd,R0,signed12` for a
constant in the signed 12-bit range; otherwise it emits `LUI` followed by
`ORI` for the low 12 bits. The two-instruction form constructs every 32-bit
constant without adding an architectural instruction.

## 4. Arithmetic, logic, shifts, and comparisons

`ADD`, `ADDI`, `ADC`, `SUB`, `SUBI`, and `SBC` update CF and OF. For addition,
CF is the carry out of bit 31; for subtraction, CF is 1 when an unsigned
borrow occurs. ADC/SBC include the old CF as carry/borrow input and then
replace CF/OF with the result flags.

OF is set when the signed result cannot be represented in 32 bits. All result
writes are modulo 2^32 after overflow calculation.

`MUL` writes the low 32 bits of signed multiplication. `MULH` writes the high
32 signed bits and `MULHU` the high 32 unsigned bits. `DIV`/`REM` are signed;
`DIVU`/`REMU` are unsigned. Signed division truncates toward zero and
`REM = dividend - quotient * divisor`. Division by zero raises vector 10.
`0x80000000 / 0xffffffff` raises vector 11 and writes no result. Multiply and
divide leave CF/OF unchanged.

AND/OR/XOR/NOT and all shifts/rotates leave CF/OF unchanged. Register shift
counts use the low five bits of `rb`, so counts are 0-31. Immediate shifts
use the five-bit immediate. `SLL`, `SRL`, and `SRA` are logical-left,
logical-right, and arithmetic-right shifts. `ROL` and `ROR` rotate by the
same count; count zero returns the input unchanged.

`SEQ`, `SNE`, `SLT`, `SGE`, `SLTU`, and `SGEU` write canonical 0 or 1 and do
not modify CF/OF. There is no CMP instruction and no condition-code dependency
for comparisons or branches.

## 5. Memory and alignment

The architecture is strict load/store. Natural alignment is required:

| Access | Required alignment |
| --- | --- |
| byte | any address |
| halfword | address divisible by 2 |
| word | address divisible by 4 |
| instruction fetch/target | address divisible by 4 |

Misaligned instruction targets/fetches use vector 2. Misaligned halfword and
word loads use vector 3. Misaligned stores also use vector 3 and perform no
partial write. Translation and permission checks occur before any load/store
architectural effect.

## 6. Exceptions and fixed frame

Exceptions are precise. The CPU constructs a fixed 16-byte frame on the
Supervisor stack. The frame is allocated by subtracting 16 from the current
`SSP`; after allocation, `SSP` points to the frame:

| Frame offset | Contents |
| ---: | --- |
| `SSP+0x00` | saved PC |
| `SSP+0x04` | complete pre-exception SR |
| `SSP+0x08` | cause/vector number |
| `SSP+0x0C` | exception-specific Info |

Each field is one little-endian 32-bit word. The saved PC is the faulting
instruction for synchronous instruction/data/privilege/arithmetic faults and
is the next instruction for an asynchronous IRQ. The saved SR is always the
complete SR value from before entry, including its old S bit and IPL.

Entry ordering is architecturally atomic with respect to guest instruction
execution:

1. Capture the pre-exception PC and SR and determine cause/info.
2. Enter Supervisor mode and select SSP through R13.
3. Subtract 16 from SSP and write the four frame words.
4. For an IRQ, set live IPL to the accepted IRQ level; for a synchronous
   exception, preserve the old IPL. Live S is 1.
5. Read the handler address from the physical vector table described below.
6. Validate that the handler address is 4-byte aligned and begin handler
   execution at that address.

The old architectural effects of the faulting instruction are not committed.
For retryable faults, RFE restores the saved PC and SR. An exception caused by
an invalid exception-stack write or an unaligned/malformed handler vector is
not recursively vectored; the CPU enters a halted error state requiring
reset. This prevents exception recursion without adding another architectural
exception class. Software must provide a valid writable Supervisor stack and
aligned handlers.

`RFE` is privileged. It reads the frame at current SSP, restores SR and PC,
then adds 16 to SSP. The mode switch caused by restored SR.S changes the
visible R13 after the frame has been removed. RFE validates the restored PC;
an unaligned restored PC raises vector 2 without consuming the frame.

### Vector numbers

The vector table has 256 entries, each a little-endian 32-bit handler address.
`VBR` is a physical, 1 KiB-aligned base. Entry `n` is at physical
`VBR + n*4`; the handler address itself is then fetched as an instruction
address under the current MMU and Supervisor permissions.

| Vector | Cause |
| ---: | --- |
| 0 | illegal instruction |
| 1 | privilege violation |
| 2 | instruction alignment |
| 3 | data alignment |
| 4 | instruction page not present |
| 5 | instruction protection |
| 6 | load page not present |
| 7 | load protection |
| 8 | store page not present |
| 9 | store protection |
| 10 | divide by zero |
| 11 | arithmetic overflow |
| 12 | syscall |
| 13 | breakpoint/debug |
| 14-15 | reserved |
| 16-22 | IRQ1-IRQ7 respectively |
| 23-255 | reserved |

For page faults, Info is the faulting virtual address. For instruction
alignment, Info is the offending target/fetch address. For illegal
instruction, Info is the offending 32-bit instruction. For syscall, Info is
the 26-bit syscall immediate. For IRQ n, Info is the IRQ level n. Other
causes use Info=0.

## 7. Hardware interrupts

IRQ1-IRQ7 are external, level-sensitive inputs. An IRQ is eligible exactly
when `IRQ level > SR.IPL`; the highest eligible asserted level wins. IRQ7 is
not an NMI, and IPL=7 masks every ordinary IRQ.

An interrupt is accepted only between instructions. It saves `PC+4` (the next
instruction), saves the complete pre-interrupt SR, enters Supervisor mode,
sets live IPL to the accepted level, and vectors through `16 + (level-1)`.
The previous IPL is restored by RFE. Higher-priority interrupts may nest;
lower or equal levels cannot pre-empt the current handler.

HALT stops normal execution and wakes only for an eligible IRQ or reset. A
masked IRQ does not wake it. The CPU has no interrupt-enable flag, NMI, or
hardware task-switch instruction.

## 8. System instructions and registers

System primary opcode `0x09` uses `sysop` values:

| `sysop` | Instruction | Encoding use |
| ---: | --- | --- |
| 0 | `MFSR Rd,SysReg` | `reg=Rd`, `sysreg` selected |
| 1 | `MTSR SysReg,Rs` | `reg=Rs`, `sysreg` selected |
| 2 | `RFE` | all operand fields zero |
| 3 | `HALT` | all operand fields zero |
| 4 | `BREAK` | all operand fields zero |
| 5-15 | reserved | illegal instruction |

System-register IDs are:

| ID | Name | Width/constraints |
| ---: | --- | --- |
| `0x00` | `SR` | 32-bit; reserved bits read zero |
| `0x01` | `USP` | 32-bit |
| `0x02` | `SSP` | 32-bit |
| `0x03` | `VBR` | physical, 1 KiB aligned |
| `0x04` | `PTBR` | physical, 4 KiB aligned |
| `0x05` | `MMCR` | bit 0 `EN`; other bits reserved |
| `0x06-0x3F` | reserved | illegal system-register selector |

All MFSR/MTSR, RFE, HALT, BREAK, and TLBFLUSH operations are privileged.
MTSR writes reserved SR/MMCR bits as zero/ignored. Misaligned PTBR or VBR
writes raise data-alignment vector 3 before modifying the register. Changing
PTBR invalidates all TLB entries; changing VBR does not require a TLB flush.
Writing SR.S is permitted to Supervisor software and changes the visible SP
after the instruction completes.

Primary opcode `0x0A` is `SYSCALL immediate26`; it is legal in either mode
and raises vector 12. The CPU does not interpret syscall numbers. Primary
opcode `0x0B` is TLBFLUSH: `page=0` invalidates all translations, and
`page=1` invalidates the translation for the virtual page containing `Ra`.
The `ra` field is required for page-specific flush and zero for all-flush.

## 9. MMU

When `MMCR.EN=0`, virtual addresses are used as physical addresses and no
page-table permission checks occur. Alignment and privilege rules still
apply. When enabled, addresses use a two-level 4 KiB page walk:

```text
VA[31:22]  page-directory index (10 bits)
VA[21:12]  page-table index (10 bits)
VA[11:0]   page offset (12 bits)
```

PTBR contains the physical address of a 4 KiB-aligned 1024-entry page
directory. A PDE contains the physical address of a 4 KiB page table plus the
flags below. The PTE contains the physical address of the 4 KiB data/code
page plus the same flags. Page-table reads use physical addresses.

| Bit | Name | Meaning |
| ---: | --- | --- |
| 0 | P | present |
| 1 | U | user accessible |
| 2 | R | readable |
| 3 | W | writable |
| 4 | X | executable |
| 5 | A | accessed |
| 6 | D | dirty |
| 7-11 | reserved | must be zero |
| 12-31 | physical page address | physical page base bits |

Effective permissions are the intersection of PDE and PTE R/W/X. User-mode
access additionally requires U in both entries; Supervisor access may use
either U value but still requires the relevant R/W/X permission. Instruction
fetch requires X, loads require R, and stores require W. P=0 produces the
appropriate not-present fault. A present entry with reserved bits set or an
insufficient permission produces the appropriate protection fault.

The MMU sets A in both traversed entries after a successful translation. It
sets D in both traversed entries after a successful store. Failed accesses do
not update A or D. A and D are ordinary software-visible memory bits and are
cleared by software if desired; the MMU does not clear them. There are no
global entries, huge pages, legacy paging modes, or hardware process concepts.

Instruction fetches and data accesses are precise: translation, alignment,
permission, and required A/D updates complete before the instruction effect
is committed. Page-fault Info is always the original virtual address.

## 10. Syscall, breakpoint, and privilege behavior

`SYSCALL` records the immediate in Info and leaves argument/result convention
to the eventual ABI/operating system. `BREAK` records vector 13 with Info=0.
An illegal or reserved encoding records the complete offending instruction.
Any privileged operation in User mode raises vector 1 with the faulting PC;
it does not partially alter system state.

## 11. Reset

Reset is not vector 0 and is not maskable. It reads two little-endian 32-bit
words using physical memory with translation disabled:

```text
physical 0x00000000  initial SSP
physical 0x00000004  initial PC
```

Both values must be 4-byte aligned. A malformed reset value places the CPU in
the same deterministic halted error state used for malformed exception entry;
reset does not recursively raise an exception.

The deterministic reset state is:

```text
mode/S              Supervisor / 1
IPL                 7
CF, OF              0
R0-R15              0, except R13 reflects SSP after loading it
PC                  physical reset word at 0x00000004
USP                 0
SSP                 physical reset word at 0x00000000
VBR                 0
PTBR                0
MMCR.EN             0
halted              false
```

R0 remains hardwired zero. Reset does not modify RAM, page tables, or the
vector table. The first instruction fetch is physical; later execution can
enable the MMU with MTSR.

## 12. Protected multitasking model

The CPU has no process, thread, or task-switch instruction. An operating
system can implement pre-emptive multitasking with timer IRQs, Supervisor
exception frames, GPR save/restore, USP/SSP, PTBR, and RFE. A software context
therefore includes R1-R15, USP, SSP, PTBR, and the suspended frame/PC/SR plus
OS-defined state. Changing PTBR invalidates all translations.

## 13. Toolchain and firmware scope

The assembler, linker/object format, debug metadata, firmware image format,
system-call ABI, and operating-system boot protocol are TBD. The 2.0 CPU
specification does not require MyEmulator 1.0 assembly compatibility, MyFS
compatibility, or firmware compatibility. Pseudo-instructions such as `LI`,
`RET`, and `CALL` are assembler conveniences and do not add architectural
state.

## 14. 1.0 lessons, not 2.0 requirements

The completed 1.0 implementation demonstrated several design pressures:

- Four small data registers and separate address registers made larger
  routines register-starved and increased save/restore traffic.
- LR, SP, and PC as distinct concepts worked, but a single LR required
  explicit stack discipline for nested calls.
- Fixed 16-bit instructions were easy to fetch and disassemble but made
  three-register operations, immediate width, and register/immediate form
  allocation tight.
- Indirect control transfer exposed the need for explicit instruction
  alignment exceptions rather than masking bad targets.
- Level-sensitive IRQs plus IPL made nesting clear, but devices must deassert
  their source after service to avoid retriggering.
- QEMU MemoryRegion devices, a simple per-level IRQ input, virtual-time timer,
  console, and floppy provided useful hardware without CPU special cases.
- RIKMON, the sector loader, MyFS, and COMMAND.COM demonstrated the value of
  symbolic assembler constants, includes, debug maps, a native debugger, and
  careful memory budgeting.

These observations inform design discussion only. They do not prescribe the
2.0 register model, encodings, peripherals, or software environment.

## 15. Implementation boundary

This milestone defines the architecture only. It adds no QEMU CPU target,
machine, translator, disassembler, assembler backend, firmware, or runtime
execution. The next implementation task must use this document and the JSON
manifest as authority and must not make new architectural decisions silently.
