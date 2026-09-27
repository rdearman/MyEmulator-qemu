# REM Assembly Programmer Reference

This guide intentionally records only verified REM information. For the full instruction reference, see `../MYEMULATOR2_ASSEMBLY_PROGRAMMING_MANUAL.md` and `../MYEMULATOR_2_ABI.md`.

## Calling convention

**VERIFIED:** REM user code uses four register argument values (`r1`–`r4`) in the current ABI. The stack pointer must be 16-byte aligned at every call boundary. Preserve callee-saved registers according to the ABI manual.

## Registers and instructions

Use the authoritative assembly manual for the complete register and instruction tables. Do not infer undocumented encodings from unrelated host assemblers.

## Memory model

REM user programs use 32-bit little-endian addresses. Loads and stores access mapped user memory; page permissions and alignment are enforced by the Linux/MMU integration. The exact memory map is documented in `../MYEMULATOR_2_ARCHITECTURE.md` and `../memory_map.md`.

## Syscalls

The current syscall numbers and argument conventions are listed in `../REM_CURRENT_SYSCALL_REFERENCE.md` and the syscall cookbook in `../REM_ASSEMBLY_SYSCALL_COOKBOOK.md` where present. Check the current reference rather than copying a number from another architecture.

## Minimal program shape

```asm
.global _start
.text
_start:
    # Follow the ABI and syscall reference for arguments and syscall entry.
    # Assemble with the REM native as and link with the REM linker script.
```

The exact syscall instruction and register clobbers are intentionally left to the verified manual rather than repeated here.

## Examples and future work

See `../MYEMULATOR2_ASSEMBLY_PROGRAMMING_MANUAL.md` examples and `../toolchain/examples/` from the repository. **FUTURE:** add a compact flight-ready example set with source, expected output, and verified hashes.
