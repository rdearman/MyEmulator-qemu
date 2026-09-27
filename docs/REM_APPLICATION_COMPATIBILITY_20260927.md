# REM application compatibility probe — 2026-09-27

## Scope and tuple

These probes ran only against disposable copies of the Yash validation rootfs.
No production image, app binary, kernel, libc, or Samurai artifact was changed.
The tests used:

| Input | SHA-256 |
|---|---|
| QEMU `REM-FLIGHT-TTY/qemu/desktop/qemu-system-myemulator32` | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` |
| Kernel `/tmp/rem-yash-unified-build/vmlinux` | `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a` |
| Seed rootfs `/tmp/rem-yash-qual/flight-yash.ext4` | `8649389a39aefa45d9a1ed0b2b8438cde5396cbb1fe1991ee26d4633cff48d59` |

## Results

| Application / workload | Result | Evidence and classification |
|---|---|---|
| Vim 9.1 startup | PASS | `/usr/bin/vim --version` returned 0 in `/tmp/rem-app-compat-vim-promptfix-20260927/console.log`. The binary SHA-256 is `35874ba096f8036a15ae07a09985a5182ea86eacde8d287bae63138812ea06c4`. |
| Vim Ex-mode edit/save | TIMEOUT | `vim -u NONE -i NONE -n -es -c "call append(0, 'REM_VIM_BATCH')" -c "wq" /root/vim-batch.txt` did not return `VIM_BATCH_RC` within the 120-second test deadline. No user fault, segmentation fault, or kernel panic was recorded. TTY trace shows Vim pid 22 issued `ioctl(fd=-1, TIOCGPGRP=0x5410)`, which returned `-EBADF` (`-9`); this alone does not establish the cause. The most specific current layer is terminal/TTY interaction or application control flow, still unresolved. The first test attempt timed out before launching Vim because the stock harness expected an uncolored `\n# ` prompt; a temporary `/tmp` harness copy matched the ANSI-colored prompt and got past startup. |
| GNU Emacs batch | CRASH (139) | `/usr/bin/emacs --batch -Q --eval '(princ "EMACS_OK")'` caused `MYEMU_BAD_USER_FAULT`: `pc=0x0210e7d8`, `addr=0x00000c40`, `fp=0x00000c80`; shell reported segmentation fault. Binary SHA-256 `b5f1d6b3ad35d3931aa9d24053e9a734a99f41d7870a48150094a20177fbc709`. Disassembly places the fault in `main+0x2118`, on a stack-frame store. This is a user-space frame/ABI or binary-runtime failure; there is no evidence of a missing syscall or filesystem failure. The old binary's compiler/libc provenance is not established, so a specific compiler-versus-libc cause remains open. |
| GNU Coreutils `cp` | CRASH (139) | `/usr/bin/cp /tmp/rem-userspace-touched /tmp/rem-userspace-copied` caused `MYEMU_BAD_USER_FAULT`: `pc=0x0200b868`, `addr=0x0000030c`, `fp=0x00000320`; shell reported segmentation fault. Binary SHA-256 `3933c6f154266fabdfe0fe2dba3bf6840cf0cf1f787af8a423ee01ecff762900`. `myemulator2-elf-objdump` identifies the faulting instruction as `lw r9,0(r12)` at `copy+0x124`; disassembly shows `r12` derived from the frame register, whose reported value is near null. This points to a userspace frame/ABI or binary-runtime failure, not a filesystem return or missing syscall. Build provenance is unknown, so do not treat this as proof of a kernel or compiler defect. |

The Emacs and Coreutils runs reached their application commands under the same
QEMU/kernel/rootfs tuple and completed with guest exit status 139. Logs and
machine-readable results are retained under:

- `/tmp/rem-app-compat-emacs-20260927/`
- `/tmp/rem-app-compat-coreutils-20260927/`
- `/tmp/rem-app-compat-vim-20260927/` (stock prompt-matcher timeout)
- `/tmp/rem-app-compat-vim-promptfix-20260927/` (temporary harness copy)

## Layer assessment

- **Compiler ABI / user binary:** Emacs and `cp` fault while accessing frame-relative data at low addresses. The failing binaries do not have reliable matching compiler provenance. Rebuild each with the accepted REM compiler/runtime tuple before attributing the failure to the kernel or libc.
- **TTY/syscall:** Vim starts and reports its version, but its headless edit/save command does not finish. An invalid-fd `TIOCGPGRP` probe returns `EBADF`; capture a focused syscall/terminal trace before changing kernel behavior.
- **Filesystem:** The `cp` failure occurs before a successful copy and is a user-memory fault; no filesystem error was logged. The test does not show that ext4 caused the crash.
- **Kernel:** Both memory faults are delivered as user faults; neither run demonstrates a missing syscall. No kernel fix is justified by this evidence.

## Next probes

1. Rebuild Vim, Emacs and GNU Coreutils from identified sources with the
   accepted compiler and matching musl/libgcc/CRT tuple; rerun in fresh
   disposable copies.
2. For Vim, trace the hanging Ex-mode process's blocking syscall and retry
   with a deterministic noninteractive stdin/TTY setup. Keep the ANSI-aware
   prompt fix confined to the test harness.
3. Probe `/usr/bin/mv`, `grep`, and `sed` from the same seed with the marker
   runner, recording each binary hash and fault site. These are not yet
   retested in this report.
4. Consider ncurses-dependent applications only after confirming an installed
   app and its terminfo/runtime files; no ncurses application was exercised in
   this pass.

This is a compatibility report, not an acceptance result for the flight image
or Samurai.
