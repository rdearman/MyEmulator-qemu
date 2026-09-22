# REM Linux signal handling

The MyEmulator2 port now has architecture signal delivery in the tracked
overlay under `linux/arch/myemulator2`. The implementation uses the existing
16 GPRs, PC, SR, user stack, and native exception frame; no CPU instruction or
architectural register was added.

## Implemented path

On return from a user exception, the architecture preserves the interrupted
`pt_regs` and calls `arch_do_signal_or_restart()`. Linux's `get_signal()` and
`signal_setup_done()` provide the normal default dispositions, blocked-mask
handling, ignored/default `SIGCHLD` behavior, `SA_SIGINFO`, `SA_RESTART`,
`SA_NODEFER`, `SA_RESETHAND`, and alternate-stack policy. The dispatcher also
routes `kill`, `tkill`, `tgkill`, and `sigaltstack` through the generic kernel
implementations; `rt_sigaction` and `rt_sigprocmask` were already present.

The signal frame contains `siginfo_t`, a Linux `ucontext`, the complete
MyEmulator2 user register image, and a two-instruction user trampoline. The
trampoline loads syscall 139 (`rt_sigreturn`) into `r1` and executes the
architectural `syscall 0` instruction. `rt_sigreturn` validates the user
frame, restores the saved signal mask and alternate stack, rejects supervisor
mode in the restored status, and restores the interrupted context.

The exception entry now reserves 96 bytes for the 84-byte software register
image plus ABI stack padding. `orig_r1` records the syscall number before the
syscall result overwrites `r1`; this enables Linux restart classes
`ERESTARTNOINTR`, `ERESTARTSYS`, `ERESTARTNOHAND`, and `ERESTART_RESTARTBLOCK`
to resume at the original syscall instruction or return `EINTR` according to
the installed handler's `SA_RESTART` flag.

## Isolated build and tests

The pinned Linux 6.12.1 source is prepared in `.linux-build/linux-6.12.1`
and the output is built in `.linux-build/build`; these paths are ignored and
are local to this clone:

```sh
MYEMU_LINUX_VERSION=6.12.1 ./toolchain/scripts/configure-linux.sh
JOBS=2 ./toolchain/scripts/build-linux.sh
./toolchain/scripts/test-linux-signal.py
```

The focused test checks the signal ABI source invariants and verifies that the
two signal entry points are present in the resulting `vmlinux`. This is a
compile/link test only. No isolated REM QEMU runtime test is claimed here:
the kernel's userspace signal path still depends on the ongoing page-fault
investigation around `0x10000000`, a working BusyBox/init environment, and a
separate QEMU instance. The build and test never use or modify
`~/Development/Active/MyEmulator-qemu`.
