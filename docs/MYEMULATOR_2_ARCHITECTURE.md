# MyEmulator 2.0 Architecture

**Status: DESIGN / NOT IMPLEMENTED**

This document is the initial design workspace for a future 32-bit MyEmulator
architecture. It is deliberately not an ISA specification. MyEmulator 1.0 is
the completed 8-bit architecture and remains the reference implementation in
this repository.

## Design goals

TBD. The 2.0 goals will be agreed before CPU implementation begins. They
should include a clean 32-bit programming model, an implementable QEMU target,
and a toolchain that supports substantial firmware and software without
being constrained by 1.0 source compatibility.

## Data model

TBD. Fundamental choices including integer widths, address width, data
alignment, byte order, and instruction size are intentionally undecided.

## Register architecture

TBD. Register count, names, special registers, status representation, and
reset values must be designed rather than mechanically widening 1.0.

## Instruction encoding

TBD. Encoding width, opcode allocation, register fields, immediate forms, and
extension space are open questions.

## Addressing

TBD. Load/store addressing, immediate addressing, address formation, and
alignment rules are not yet specified.

## ALU

TBD. Arithmetic, logical operations, shifts, flag effects, and overflow/carry
semantics require an explicit 2.0 design.

## Control transfer

TBD. Branches, direct and indirect calls, returns, link state, and control
transfer alignment have not been selected.

## Stack

TBD. Stack direction, width, alignment, calling convention, and interrupt
frame interaction are open.

## Interrupts

TBD. Priority levels, masking, pending state, entry, nesting, and device
interfaces must be designed for 2.0.

## Exceptions

TBD. Synchronous faults, vectors, saved state, causes, and return semantics
are not inherited automatically from 1.0.

## Reset

TBD. Reset state, vectoring, firmware ownership, and startup sequencing are
undecided.

## Memory map

TBD. Address-space size, RAM, ROM, vectors, MMIO, and reserved regions will
be defined after the CPU model is chosen.

## MMIO

TBD. Device register width, access rules, discovery, and interrupt wiring are
open. Existing 1.0 peripheral ideas may inform the design but do not constrain
it.

## Privilege

TBD. No privilege, user mode, protection, or capability model has been
selected.

## Calling convention

TBD. Register preservation, argument passing, return values, stack frames,
and unwind metadata require a deliberate ABI design.

## Toolchain

TBD. The assembler, linker/object format, debug metadata, debugger, and
firmware build workflow will be selected after the ISA is defined.

## Firmware

TBD. Firmware format, reset entry, ROM layout, monitor responsibilities, and
boot protocol are undecided.

## Storage and boot

TBD. Storage devices, filesystem compatibility, boot stages, and executable
formats are open questions. MyFS compatibility is not required.

## Lessons from MyEmulator 1.0

These are observations from the completed implementation, not requirements
for 2.0.

### Programming model

- Four 8-bit data registers (`R0`-`R3`) created pressure in routines that
  needed simultaneous values, counters, pointers, and temporary state.
- Separate 16-bit address registers (`A0`-`A3`) made byte-addressed memory
  operations understandable, but moving values between data and address
  domains added instruction and register-management overhead.
- `LR`, `SP`, and `PC` worked as distinct architectural concepts. Calls with a
  single link register made nested calls depend on explicit stack discipline.
- The final `BL`/`JA`/`JLA`/`RET` model provided both PC-relative and indirect
  control transfer, while the later `JA`/`JLA` addition was driven directly by
  writing monitor and boot software.

### Encoding and execution

- Fixed 16-bit instructions were simple to fetch, decode, disassemble, and
  test, but operand fields were scarce. Immediate and register forms needed
  careful family/subencoding design; preserving all 8-bit immediates mattered
  for masks and device values.
- Indirect control transfer exposed the need for an explicit instruction
  alignment rule. Odd runtime targets cannot be silently rounded without
  hiding software errors.
- The alignment exception and its RTI frame are small and understandable,
  but exception entry and retry semantics must be planned alongside all other
  PC-loading paths in a future architecture.

### Stack, interrupts, and devices

- Pre-decrementing stack operations and full-width saved values made nested
  calls and interrupt frames possible, but firmware must carefully budget RAM
  because the stack shares the small address space.
- Level-sensitive IRQs with an IPL field made priority and nesting observable
  in a compact model. Devices must deassert their level after software clears
  the underlying condition or they can retrigger after return.
- The console, floppy controller, and timer demonstrated that ordinary QEMU
  `MemoryRegion` devices plus a simple CPU-side IRQ interface are sufficient
  for useful fictional hardware.
- Guest virtual time for the timer was important: debugger pauses must not
  consume guest time.

### Firmware and software constraints

- A small 256-byte sector-0 loader is possible, but parsing metadata and
  moving data in a four-register machine leaves little room for diagnostics.
- RIKMON exposed the practical value of symbolic assembler constants,
  includes, labels, debug maps, and readable source syntax.
- Building a read-only filesystem and shell on the ISA exposed the cost of
  repeated sector-buffer management, filename comparison, bounded line input,
  and preserving state across subroutine calls.
- The native debugger and disassembler were more useful than pretending the
  fictional CPU was a stock GDB architecture. Machine-readable debugger
  control also made Emacs integration possible.

None of these observations decides whether 2.0 should retain or replace the
corresponding 1.0 choices.

## Reuse policy

MyEmulator 2.0 may reuse peripheral concepts, device models, documentation
patterns, or carefully isolated QEMU infrastructure from 1.0 where that is
appropriate. Console, floppy/storage, and timer concepts are possible
examples.

Reuse is optional. The 2.0 CPU must not be constrained by 1.0 source or binary
compatibility. There is no requirement for compatible assembly syntax, MyFS
images, firmware, debugger register layout, or peripheral addresses.

## QEMU repository structure

The existing QEMU overlay follows QEMU's target/machine split:

```text
qemu/target/myemulator/       1.0 CPU target
qemu/hw/myemulator/           1.0 machine and devices
qemu/configs/targets/         target-list configuration
qemu/configs/devices/         device configuration
qemu/gdb-xml/                 1.0 target description
```

The build helper applies this overlay to the local upstream checkout and
currently configures only `myemulator-softmmu`. No 2.0 QEMU target directory
or placeholder CPU has been added: QEMU target registration, Meson files,
Kconfig, target configuration, and machine naming should be added together
only after the 2.0 architecture is specified. The likely eventual layout is a
parallel target and hardware namespace, but the exact names must follow the
QEMU version and target-list conventions in use when implementation starts.

## Open questions

All architectural decisions remain open, including:

- What should the 32-bit data and address model be?
- What instruction format gives useful register and immediate forms?
- Which parts of 1.0's simple firmware/MMIO model remain desirable?
- Should 2.0 share any debugger protocol or assembler infrastructure?
- What software and boot environment should be considered the first target?

These questions should be answered in design discussions before creating a
functional 2.0 CPU or machine.
