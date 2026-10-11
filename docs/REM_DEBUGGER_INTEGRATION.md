# REM32 debugger integration design

## Current pieces and their boundaries

REM32 currently has three relevant mechanisms:

- The Linux architecture ptrace implementation in
  `linux/arch/myemulator2/kernel/ptrace.c` exposes a process's saved `pt_regs`
  through `PTRACE_GETREGS`/`PTRACE_SETREGS`, supports single-step, and delegates
  normal ptrace requests to Linux. This is the right source for a selected
  guest process's stopped register context.
- The `myemulator32-debug` QMP command in
  `qemu/hw/myemulator32/myemulator32-debug.c` can read the live CPU registers,
  virtual/debug memory, or physical memory, and control IRQ/NMI inputs. It is
  a VM-level instrument; its `regs` response is the current CPU state, not a
  historical process fault frame.
- `qemu/target/myemulator32/cpu.c` sets `gdb_num_core_regs = 0`, so the
  MyEmulator32 target does not currently advertise a usable QEMU GDB register
  file. The other target's GDB XML and `myemulator-debug` command describe the
  legacy MyEmulator CPU and must not be reused as the REM32 ABI.

The Linux bad-user-page-fault record now has a stable textual family:

- `MYEMU_BAD_USER_FAULT`: pid/name, PC, fault address/cause, trap state,
  `r0`-`r15` (including aliases SP/LR/FP).
- `MYEMU_BAD_USER_FAULT_VMA`: VMA bounds/flags and brk values.
- `MYEMU_BAD_USER_FAULT_INSN`: PC, 16-byte window starting at PC-8, and a
  validity bitmask for safely sampled bytes.

The report format is present in the kernel source. The live process debugger
has separately passed its ptrace qualification, including a deliberate
debuggee fault; see [REM_DEBUGGER.md](REM_DEBUGGER.md). That qualification does
not parse or validate these three kernel log records. The record format,
partial-record handling, and a `fault last` viewer therefore remain
unqualified postmortem work, separate from the passing live debugger.

## Proposed integration

### 1. Fault record as the postmortem entry point

Provide a debugger `fault last` view that parses the three record lines into a
single snapshot. Show the process identity and tuple/build identity beside
registers; display PC, SP, LR, fault address/cause, VMA, then an instruction
window. Respect the byte-validity mask and never decode unavailable bytes as
zero instructions. Keep the original log text available so parsing errors do
not hide evidence.

Resolve symbols only against the exact executable and debug/symbol files from
the candidate manifest. The process name is a hint, not a unique executable
identity. If executable provenance cannot be matched, show raw addresses and
bytes without guessed source locations.

### 2. Live ptrace debugging (implemented and qualified)

`remdbg` launches the guest executable under Linux ptrace. At a ptrace stop,
`GETREGS` is the authoritative live register state; ptrace memory requests
inspect that process's virtual address space. The qualified commands include
continue, single-step, register and memory display, and software breakpoints.
A fault log can seed postmortem PC/register display, but it cannot resume the
already-faulted context unless the process is stopped under ptrace and its
state is still available. See the command list and exact qualification tuple
in [REM_DEBUGGER.md](REM_DEBUGGER.md).

### 3. Keep QMP as a separate VM diagnostics backend

Use `myemulator32-debug` for whole-VM inspection, CPU/MMU state, physical
memory, and device/interrupt diagnostics. Label its register response as
current CPU state and its memory mode explicitly as guest virtual/debug or
physical. It must not silently substitute live CPU registers for the saved
registers in a fault record. If a future front end combines QMP and ptrace,
show the source and capture time for each view.

### 4. Add GDB only after defining the REM32 register protocol

QEMU GDB support could provide a standard remote client, but the REM32 target
currently advertises no core registers. Before enabling it, define and test a
target description for the 32-bit `r0`-`r15`, PC, SR, and any architecturally
visible control state, plus read/write semantics and exception-stop mapping.
Do not copy the legacy 8/16-bit target description. Keep Linux ptrace useful
independently of this QEMU work.

## Suggested delivery order

1. Add a focused qualification of the kernel fault-record format against a
   deliberate user fault; keep fixture output and logs attached to the exact
   candidate manifest. The existing remdbg deliberate-fault test validates
   ptrace stop/reporting, not this log format.
2. Implement a read-only fault-record parser/view and tests for missing lines,
   partial instruction windows, and malformed fields.
3. Separately implement and qualify MyEmulator32 QEMU GDB register/stop
   semantics if a standard remote debugger client is needed.

## Evidence references

- Kernel user-fault capture: `linux/arch/myemulator2/mm/fault.c`
- Linux register and stepping path: `linux/arch/myemulator2/kernel/ptrace.c`
- QMP machine inspection: `qemu/hw/myemulator32/myemulator32-debug.c`
- REM debugger front-end concepts: `docs/Inside_REM_Edition2_Chapters17_19_revision2.md`
- Fault diagnostic fixture: `toolchain/tests/fault-diagnostic/invalid-load.S`
