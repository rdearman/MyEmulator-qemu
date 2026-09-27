# REM current target manifest

This manifest freezes the target described by the accompanying offline
documents. It is a snapshot, not a promise that later repository revisions
retain every interface.

| Item | Snapshot |
|---|---|
| CPU/QEMU target | MyEmulator2 / `myemulator32` |
| Source revision | `d463af9f6298446ca94ef0710efb42ee21ac2f7c` plus the current working-tree changes |
| Object format | ELF32 little-endian |
| Assembler/linker | `.toolchain-install/bin/myemulator2-elf-as`, `myemulator2-elf-ld` |
| QEMU | `.qemu-build/qemu-system-myemulator32` |
| Kernel image | `.linux-build/build/vmlinux` |
| Userspace linker script | `toolchain/userspace/myemulator2-user.ld` |
| Hosted-C runtime | `toolchain/flight-kit-runtime/` (staging copy of the validated tuple; integration into the selected flight image remains separate) |

The local `.musl-build/` from cached musl 1.2.5 stopped in
`src/stdio/vfwscanf.c`: the current target compiler treats its `wchar_t`
pointer-sign warning as an error. Separately, an already validated guest
runtime tuple has now been copied to `toolchain/flight-kit-runtime/`; its
artifact hashes are recorded there and in `REM_VALIDATED_RUNTIME_TUPLES.md`.
The staging directory does not prove that the selected flight image includes
those files.

The source tree is the authority for this snapshot. The phone bundle need not
contain the whole repository: copy the PDFs, the source paths named in
`REM_OFFLINE_BUILD_MANIFEST.md`, and any binaries listed as present by the
pre-flight checker.

Implemented syscall dispatch and regression coverage are separate facts. Use
`tools/generate-myemulator2-syscall-reference.py` to regenerate the syscall
table from `linux/arch/myemulator2/kernel/syscall.c` after changing the tree.
