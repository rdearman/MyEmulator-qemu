# REM ABI Version 1

**Status: DESIGN / ABI v1 candidate**

This document defines the REM software and toolchain ABI. It is
separate from the CPU specification in
[`MYEMULATOR_2_ARCHITECTURE.md`](MYEMULATOR_2_ARCHITECTURE.md) and the
machine-readable instruction manifest in
[`myemulator2-encoding.json`](myemulator2-encoding.json). The CPU documents
remain authoritative for instruction behavior and encoding; this document is
authoritative for ELF, assembler, linker, calling-convention, and deployment
interfaces.

The ABI version is independent of the CPU architecture version. The
architecture tag `myemulator2-architecture-v1` is not changed by this
document. A reviewed ABI release may later be tagged
`myemulator2-abi-v1`.

## 1. Scope and canonical formats

The canonical object and executable format is **32-bit little-endian ELF**:

```text
source.s -> myemulator2-elf-as -> ELF32 relocatable object
         -> myemulator2-elf-ld -> ELF32 static executable
         -> objcopy -O binary  -> optional raw deployment image
```

Raw binaries are derived deployment artifacts, not an alternative executable
format. Static executables are the initial ABI. Dynamic linking, shared
libraries, GOT/PLT formats, dynamic TLS, and a Linux userspace ABI are future
work.

The canonical GNU target name is `myemulator2-elf`. This is the
bare-metal/static target. A future Linux target will have a separate triplet
and will use the same ELF instruction and relocation ABI.

## 2. ELF machine identity

The project-local experimental value is:

```text
EM_MYEMULATOR2 = 0xF2E2
```

`0xF2E2` was checked against the GNU binutils 2.46.0 ELF definitions used as
the reference source base; it has no definition or occurrence there. This is
a **PROVISIONAL / PROJECT-LOCAL value, NOT AN OFFICIAL ELF ALLOCATION**.
It has no CPU architectural significance and must not be represented as an
official registry assignment. If an official value is later assigned, the
ELF ABI version must be revised rather than silently changing old objects.

ELF data encoding is `ELFDATA2LSB`, class `ELFCLASS32`, with 32-bit virtual
addresses and little-endian 32-bit words. The initial ABI uses ELF32 `RELA`
relocation sections.

## 3. Canonical GNU assembly syntax

Source uses conventional GNU assembler spelling. Mnemonics and register names
are case-insensitive; the canonical documentation spelling is lowercase.
Comments use `#` to end a line. Standard GAS directives are used where
applicable.

General registers are `r0` through `r15`. The aliases are:

```text
zero = r0
sp   = r13
lr   = r14
```

The architectural special-register names are `sr`, `usp`, `ssp`, `vbr`,
`ptbr`, `mmcr`, `dfsp`, `tp`, `time`, and `timecmp`. The 64-bit `time` and
`timecmp` values are exposed by the 32-bit system-register instruction as
explicit halves: `time_lo`, `time_hi`, `timecmp_lo`, and `timecmp_hi`. A
single `mfsr` or `mtsr` never claims to transfer 64 bits.

Examples:

```asm
.text
.global _start
_start:
        li      r1, 42
        lw      r2, 12(r5)
        sw      r2, 20(r6)
        beq     r1, r2, equal
        jal     function
        jr      lr

function:
        ret

equal:
        mfsr    r3, sr
        mtsr    tp, r4
```

Memory operands are always `displacement(base)`, including zero displacement:
`lw r1, 0(r5)`. There are no implicit address-register updates. Branches,
`j`, and `jal` take symbolic targets. `jr` and `jalr` take one register.

For this 32-bit target, `.word` emits one little-endian 32-bit value and
`.short` emits one 16-bit value. This target-specific `.word` size follows the
architectural word size while retaining normal GNU directive spelling.

The frozen pseudo-instructions are:

```text
ret              jr r14
call symbol      jal symbol
li rd, constant  one addi or a lui/ori pair, as described below
```

No push/pop, compare, or other compatibility pseudo-instructions are part of
ABI v1. Relocation modifiers use the GNU-style forms `expr` and
`expr`.

## 4. Calling convention

The initial calling convention is:

| Register | ABI role | Preservation |
| --- | --- | --- |
| `r0` | constant zero | architectural |
| `r1-r4` | first four arguments | caller-saved |
| `r1` | 32-bit return value | caller-saved |
| `r1:r2` | 64-bit return value, low word in `r1` | caller-saved |
| `r5-r12` | general values | callee-saved |
| `r13/sp` | downward-growing stack | callee-managed |
| `r14/lr` | return link | caller/callee must preserve as required by calls |
| `r15` | temporary | caller-saved |

Additional arguments are passed on the stack. The stack is 16-byte aligned at
every function-call boundary and has no red zone.

The ABI does not yet define structure-return rules, aggregate passing,
varargs register save areas, unwind encoding, or compiler-specific stack
canaries. These are listed under Future ABI Work rather than guessed here.

## 5. Relocation model

Relocations use ELF32 `RELA`. Every relocation carries an explicit signed
32-bit `r_addend`; the bytes at the relocation place contain the instruction
template or zero-initialized data field. Linkers must diagnose overflow,
misalignment, or an instruction template whose reserved bits cannot be
preserved. They must not truncate a value silently.

Notation:

```text
S = resolved symbol value
A = signed RELA addend
P = address of the relocation place, i.e. the instruction/data word address
W = little-endian 32-bit instruction word at P
```

All relocation numbers are project-local ABI numbers:

| Number | Name | Applies to |
| ---: | --- | --- |
| 0 | `R_MYEMU_NONE` | no operation |
| 1 | `R_MYEMU_32` | a 32-bit little-endian data word |
| 2 | `R_MYEMU_BRANCH13` | conditional branch `disp[12:0]` |
| 3 | `R_MYEMU_JUMP26` | `J`/`JAL` `disp[25:0]` |
| 4 | `R_MYEMU_HI20` | `LUI` immediate field `imm[20:1]` |
| 5 | `R_MYEMU_LO12` | ALU-immediate field `imm[11:0]` |

### `R_MYEMU_NONE`

No bytes are changed. `S`, `A`, and `P` are ignored.

### `R_MYEMU_32`

For a 32-bit data word, compute `V = S + A`. Require
`0 <= V <= 0xFFFFFFFF`; write `V` little-endian at `P`. This relocation is
not PC-relative and has no scaling.

### `R_MYEMU_BRANCH13`

This relocation applies only to a branch-format instruction with a preserved
opcode/condition/register template. Compute:

```text
D = S + A - (P + 4)
Q = D / 4
```

`D` must be divisible by four. `Q` must fit the signed 13-bit range
`-4096 <= Q <= 4095`. Write `Q & 0x1FFF` into `W[12:0]`, preserving every
other bit. `P + 4` is required because the CPU branches from the address of
the following instruction, not from the relocation place itself.

### `R_MYEMU_JUMP26`

This relocation applies only to `J` or `JAL`. Compute the same
`D = S + A - (P + 4)` and `Q = D / 4`. Require four-byte divisibility and the
signed 26-bit range `-33554432 <= Q <= 33554431`. Write
`Q & 0x03FFFFFF` into `W[25:0]`, preserving the primary opcode.

### `R_MYEMU_HI20` and `R_MYEMU_LO12`

For an address/value pair, both relocations use the same `V = S + A`:

```text
HI = (V >> 12) & 0xFFFFF
LO = V & 0xFFF
```

`R_MYEMU_HI20` applies to `LUI` and writes `HI << 1` into `W[20:1]`,
preserving `W[31:21]` and requiring `W[0]` to remain zero.
`R_MYEMU_LO12` applies to an ALU-immediate instruction, normally `ORI`, and
writes `LO` into `W[11:0]`. The low half is zero-extended by `ORI`, so no
RISC-style sign-adjusted high-half carry is used.

The pair constructs every 32-bit address:

```asm
        lui     r5, symbol       # R_MYEMU_HI20
        ori     r5, r5, symbol   # R_MYEMU_LO12
```

For example, if `symbol + A = 0x12345678`, the linker writes `0x12345` into
the LUI field and `0x678` into the ORI field. The CPU produces
`0x12345000 | 0x678 = 0x12345678`.

No relocation is defined for a partial load/store displacement because its
13-bit field is an offset from a runtime base register, not an absolute
address construction mechanism. Programs use the HI20/LO12 pair to construct
the base address first.

## 6. Branch and jump ranges

For an instruction at `P`, the architectural base is `P+4`. The encoded value
is the signed instruction count from that base:

```text
target = (P + 4) + sign_extend(encoded_disp) * 4
```

Thus a branch at `P` can reach `(P+4)-16384` through `(P+4)+16380`, and a
`J`/`JAL` can reach `(P+4)-134217728` through `(P+4)+134217724`. The target
must be four-byte aligned. The linker rejects values outside those ranges;
it does not wrap them.

## 7. Machine physical memory ABI

The current QEMU 2.0 bring-up machine maps configurable RAM at physical zero
and loads raw `-kernel` bytes there. That is a development loader, not the
long-term ABI. The stable physical layout reserved by ABI v1 is:

| Range | ABI meaning |
| --- | --- |
| `0x00000000-0x000003FF` | 256-entry physical vector table |
| `0x00000400` | initial SSP word |
| `0x00000404` | initial PC word |
| `0x00000408-0x00000FFF` | reserved bootstrap metadata |
| `0x00001000-0xEFFFFFFF` | physical RAM address space, subject to installed RAM size |
| `0xF0000000-0xF0FFFFFF` | reserved MMIO window |
| `0xF1000000-0xFEFFFFFF` | reserved future machine space |
| `0xFF000000-0xFFFFFFFF` | firmware ROM window, reserved for RIKMON firmware |

This does not claim that all 3.75 GiB of RAM is installed. The current
machine installs 16 MiB and has no 2.0 MMIO devices; future machine versions
must preserve the reset words and vector-table meaning. Future console,
timer, and block devices must be assigned subranges inside the MMIO window,
not by taking RAM addresses or vector entries.

The future block-storage controller is expected to receive a dedicated MMIO
subrange, for example within `0xF0100000-0xF01FFFFF`. This is a reservation,
not a device definition. Filesystem format is not part of this ABI.

## 8. RIKMON residency and bare-metal layout

The existing 1.0 RIKMON is a separate 16-bit firmware and remains unchanged.
It uses the 1.0 ROM/MMIO map and is not a 2.0 binary.

The proposed 2.0 firmware contract places a future 2.0 RIKMON image in
`0xFF000000-0xFFFFFFFF`, with its reset entry at `0xFF000000`. The reset
words at `0x400` and `0x404` will contain the initial SSP and that entry
address. With the current 16 MiB RAM configuration, the recommended initial
SSP is `0x00FFF000`; firmware must obtain it from the reset word rather than
hard-code it.

The initial bare-metal linker script places software loaded by RIKMON at
`0x00100000`:

```text
.text      0x00100000, 4-byte aligned
.rodata    after .text, 16-byte aligned
.data      after .rodata, 16-byte aligned
.bss       after .data, 16-byte aligned, NOBITS
```

The linker defines `_start` as the entry symbol, plus
`__text_start/__text_end`, `__rodata_start/__rodata_end`,
`__data_start/__data_end`, `__bss_start/__bss_end`, and `__image_end`.
The linker does not allocate a heap or reserve a red zone. `_start` owns
startup initialization of `.bss` and any runtime stack policy.

The load address is physical while MMU-disabled bare-metal code runs. A
future MMU-enabled environment may use different virtual addresses while
retaining the same ELF relocation and instruction ABI.

## 9. Raw-image contract

An ELF32 executable is the canonical artifact. When a raw image is required,
`objcopy -O binary` emits the contents of allocatable `PT_LOAD` sections from
the lowest load address through the highest file-backed byte. Gaps are filled
with zero. `.bss` is `NOBITS` and contributes no file bytes; a loader must
zero `[__bss_start, __bss_end)`.

The raw image's first byte corresponds to its lowest linked load address; it
does not implicitly become a reset vector, and `objcopy` does not encode the
ELF entry point. A deployment wrapper or loader must place it at the linked
load address and transfer control to the ELF entry point. The current QEMU
`-kernel` path does neither ELF parsing nor arbitrary load-address handling,
so it is not the canonical way to run ABI-v1 executables. Future QEMU and
RIKMON loaders must consume the same ELF32 file and honor its program
headers, entry point, writable segments, and BSS requirements.

## 10. Future ELF loading and storage

QEMU development loading, RIKMON disk loading, and eventual Linux userspace
loading are intended to consume the same ABI-v1 ELF32 format. RIKMON will
eventually need an ELF program-header loader, a block-device driver, and
enough filesystem support to locate files. A future QEMU block device may
back a raw or qcow2 image containing ext4, but neither the block protocol nor
ext4 is defined here.

## 11. Binutils 2.46.0 implementation boundary

The reference implementation base is GNU binutils 2.46.0. A later port will
need target support in at least:

```text
include/elf, include/opcode
bfd
gas/config
opcodes
ld and its default linker emulation/script
target/configuration tables where required by the selected release
```

The port must implement the complete instruction manifest, the six
relocations above, the aliases `li`, `ret`, and `call`, and matching objdump
disassembly. It must not add CPU instructions or dynamic-linking machinery.

## 12. Future ABI work

The following are intentionally not ABI-v1 decisions:

- aggregate, structure-return, and varargs calling details;
- DWARF register-number assignments and unwind information;
- syscall numbers and the userspace syscall ABI;
- Linux virtual-address layout and process startup stack;
- dynamic linking, GOT/PLT, shared objects, and TLS relocation models;
- official ELF machine-number allocation;
- exact MMIO register maps for future 2.0 devices;
- filesystem and block-device formats;
- C language type sizes beyond the architectural 32-bit word direction.

## Decisions for human review

- Approve the project-local `EM_MYEMULATOR2 = 0xF2E2` value and its
  explicitly provisional status.
- Approve ELF32 `RELA` rather than `REL`.
- Approve the six relocation names/numbers and the `P+4`, instruction-unit
  PC-relative formulas.
- Approve the 2.0 physical map, including the proposed RIKMON ROM window and
  `0x00100000` bare-metal load address.
- Approve the GNU memory syntax `displacement(base)` and the target's
  direct-symbol HI20/LO12 relocations (`lui rN, symbol`; `ori rN, rN, symbol`).
- Approve the initial bare-metal linker symbols and section alignment rules.
