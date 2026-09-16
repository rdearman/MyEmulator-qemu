# QEMU Implementation Plan

This project should implement MyEmulator as a real QEMU target and machine, not
as a wrapper around the Python emulator.

The implementation should be developed from the architecture documented in this
directory, using `rdearman/MyEmulator` as a behavioural reference. When the
Python emulator conflicts with design intent or old software evidence, follow
`docs/architecture-decisions.md`.

## Upstream QEMU Snapshot Inspected

QEMU source inspected through the official GitHub mirror `qemu/qemu` on
2026-09-16.

- Default branch: `master`.
- Repository status: official mirror, pushed 2026-09-16.
- Relevant reference paths:
  - `target/avr/*`
  - `hw/avr/*`
  - `target/rx/*`
  - `hw/rx/*`
  - `configs/targets/avr-softmmu.mak`
  - `configs/targets/rx-softmmu.mak`

## Reference Targets

Primary reference: AVR.

Why:

- Softmmu-only 8-bit CPU target.
- 16-bit instruction fetch path.
- Compact CPU state with byte registers, `PC`, `SP`, and flags.
- Uses QEMU decodetree via `target/avr/insn.decode`.
- Has simple board examples under `hw/avr`.
- Handles separate conceptual code/data spaces, which may become relevant if
  MyEmulator's EPROM/RAM design is revived.

Secondary reference: RX.

Why:

- Modern translator-loop style with clear `DisasContext`, global TCG registers,
  helper calls, and machine image loading.
- `hw/rx/rx-gdbsim.c` is a useful compact example for a simple machine that
  wires RAM, an MCU/CPU object, and loader behaviour.

Avoid using large modern targets such as x86, ARM, RISC-V, or PowerPC as the
primary structural model. They carry architectural complexity unrelated to this
machine.

## Proposed QEMU Target Shape

Target name: `myemulator`.

Initial scope:

- Softmmu-only.
- TCG only.
- One CPU model.
- 8-bit general registers `r[4]`.
- 8-bit `lr`.
- 16-bit `pc`.
- 16-bit `sp`.
- Flags `zf`, `of`, `cf`.
- Halt/running state represented through normal QEMU CPU halt/exception
  mechanisms, not a Python-style `interrupt_flag`.

Possible source layout inside a QEMU tree:

```
target/myemulator/
    cpu-param.h
    cpu-qom.h
    cpu.c
    cpu.h
    gdbstub.c
    helper.c
    helper.h
    insns.decode
    meson.build
    translate.c

hw/myemulator/
    Kconfig
    meson.build
    myemulator-machine.c
    myemulator-console.c        # later
    myemulator-storage.c        # later

configs/targets/
    myemulator-softmmu.mak
```

Repository-local planning/testing layout before importing QEMU source:

```
docs/
reference/
    extracted-tests/            # selected old programs/test vectors only
tools/
    convert-myemulator-hex.py   # later, old .hex to raw/QEMU-loadable image
tests/
    differential/
```

Do not vendor the entire old Python emulator or a full QEMU source checkout
into this repository.

## Decoder and Translation

Use QEMU decodetree for the 16-bit instruction format:

```
@rr_imm .... .... ........  rd=%rd rn=%rn imm=%imm8
LI     0001 .... ........  @rr_imm
ADD    0011 .... ........  @rr_imm
...
```

The translator should:

1. Fetch a 16-bit instruction.
2. Decode opcode, `Rd`, `Rn`, `imm8`.
3. Emit TCG for each implemented instruction.
4. Update architectural `PC`.
5. End translation blocks on control flow, syscall/trap, halt, and exceptional
   conditions.

Open design point:

- Whether QEMU memory fetch uses byte-addressed `PC`, `PC * 2` with
  instruction-slot `PC`, or another explicit code-address convention. This is
  UNRESOLVED. Existing assembler/program evidence must be considered before the
  representation is frozen.

Branch and jump semantics:

- QEMU should implement normal intended target semantics: taken control flow
  sets `PC` to the encoded target, with no hidden post-branch `+1`.
- Old binaries assembled with label operands may encode `target - 1`; handle
  that, if required, as compatibility tooling rather than CPU semantics.

Unsupported opcodes:

- Initial bring-up should stop execution with an obvious diagnostic.
- Do not preserve the Python emulator's accidental double `PC` increment.

## Machine Definition

Start with a minimal board:

- One CPU.
- 64 KiB RAM.
- Optional `-kernel` raw loader at address 0.
- Initial `PC = 0`.
- Initial `SP = 0xffff`.
- No ROM, no storage, no timer.
- A temporary test halt mechanism can be implemented as an illegal/reserved
  pattern or by treating `PC == 0xff` as compatibility halt, but this must be
  documented as provisional.

Later:

- Add a ROM/EPROM region once boot semantics are decided.
- Add a chardev-backed console device.
- Decide whether the `POP` bit-7 escape becomes a real trap/syscall mechanism,
  semihosting compatibility hook, or gives way to MMIO/serial devices.
- Add storage only after deciding whether to preserve the host-folder model as
  compatibility tooling or design a real block device.
- Add timer/interrupt controller only when software requires it.

## Differential Testing Strategy

Build a reference harness that can run small programs on:

- Python `rdearman/MyEmulator`.
- QEMU `myemulator`.

Compare:

- `R0`-`R3`, `LR`, `PC`, `SP`.
- `ZF`, `OF`, `CF`.
- Selected memory ranges.
- Console output.
- Halt/exception reason.

Initial test vectors should be extracted, not wholesale copied, from old tests:

- Simple `LI`.
- Arithmetic and flag cases.
- Memory load/store.
- Branch after `CMP`.
- Stack push/pop.
- `SYS_PRINT` compatibility.

## First Implementation Milestone

Smallest useful milestone:

1. Create a minimal QEMU `myemulator-softmmu` target and machine.
2. Define CPU state and reset.
3. Implement fetch/decode for 16-bit words.
4. Implement only:
   - `LI`
   - `ADD` immediate
   - `ST` or `LD`
   - provisional halt mechanism
5. Run a tiny program:

```
li  r0, #0x02
add r0, r0, #0x03
halt
```

Expected final state:

- `R0 = 0x05`
- `ZF = false`
- `CF = false`
- `OF = false`
- CPU halted for the expected reason.

The next milestone should add observable output, preferably through a QEMU
chardev-backed console plus optional compatibility support for the old
`syscall 1` print path.

## Rules for Future Work

- Do not copy the Python emulator into QEMU.
- Preserve observable compatibility, including odd behaviour, only where old
  software can observe it.
- Keep old repositories read-only.
- Record every intentional incompatibility in docs before implementing it.
- Add differential tests as each instruction group lands.
