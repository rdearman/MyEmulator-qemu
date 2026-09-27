# CODEX #3 handover

CODEX NUMBER: #3  
ASSIGNMENT: REM runtime compatibility probes  
CURRENT STATUS: COMPLETE — runtime probes and evidence recorded

## Tested tuple

- QEMU SHA-256: `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`
- Kernel (`/tmp/rem-yash-unified-build/vmlinux`) SHA-256: `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a`
- Disposable probe rootfs (`/tmp/rem-vim-final/rootfs.ext4`) post-test SHA-256: `7208a549f687cb6161590b779f2097a3e78703493603e5c56bbeaa9957813a7f`

The rootfs is a disposable clone, not the flight image. It was modified only
for probes, including adding `/dev/ptmx`; `e2fsck -fn` passed after repair.
No kernel, libc, application binary, toolchain, or production image was
modified.

## Probe sources and evidence

- [Process/session probe](toolchain/tests/rem-process-session-probe.c)
- [Terminal/PTY probe](toolchain/tests/rem-terminal-pty-probe.c)
- [VM/runtime probe](toolchain/tests/rem-vm-runtime-probe.c)
- Architecture observations: [REM_ARCHITECTURE64.md](docs/REM_ARCHITECTURE64.md), section “Runtime probes: process, PTY, mapping, signals, and timers”
- Guest logs: `/tmp/rem-runtime-compat/final-vm-pty.log` (latest); `/tmp/rem-runtime-compat/guest.log`; `/tmp/rem-runtime-compat/pty-io-guest.log`

The probes were statically built with the established REM cross compiler.
Their binaries are under `/tmp/rem-runtime-compat/`; the disposable rootfs
contains the staged copies.

## Findings and ownership

| Finding | Classification | Evidence / impact |
|---|---|---|
| `fork`, `execve`, `waitpid`, child `setpgid`, and `setsid` work. | Kernel/process | Process probe passes these operations. Yash reports session ID 0 at launch, so its launcher did not establish a session. |
| Child `_exit(24)` is reported as signal 24 (`SIGXCPU`), including after raw fork-style clone. | Kernel syscall/wait ABI | Dispatcher passes the unencoded value to `do_group_exit`; wait status is incorrect for ordinary exit codes. |
| Public libc `clone(SIGCHLD)` returns `ENOSYS`; raw syscall 220 with null new stack creates a child. | Libc/kernel boundary | Only the limited raw fork-style clone path was demonstrated. General clone semantics remain unverified. |
| TTY termios and 80x24 geometry work. `tcgetpgrp` on Yash's serial TTY returns `ENOTTY`; after `setsid` + `TIOCSCTTY`, controlling-terminal operations work. | Boot/login integration | Terminal primitives exist; session/controlling-TTY setup is missing from the tested launch path. |
| PTY open, devpts mount, termios, and bidirectional master/slave I/O work when `/dev/ptmx` exists. | Rootfs provisioning | Probe rootfs initially lacked the device node; `mknodat` syscall 33 returned `ENOSYS`. Manually staging the node allowed the PTY workflow. |
| Anonymous and read-only file-backed `mmap`, `mprotect`, and `munmap` work. `msync` returns `ENOSYS`. | Kernel syscall | REM musl uses syscall 227 for `msync`, with no dispatcher implementation on this tuple. |
| Signal handler, mask, `kill`, and `sigsuspend` work. `timer_create` (107) and `timer_settime` (409) return `ENOSYS`. | Kernel syscall | Signal delivery works independently; POSIX timer support is absent. |
| `pthread_create` returns `EAGAIN`. | Kernel/libc boundary | A candidate kernel ABI issue is clone TLS forwarding: dispatcher clone path passes TLS as zero. The probe does not prove this is the direct cause. |

OpenSSH 10.5p1 faults before reaching networking, process, signal, or PTY
workloads on the previously tested run; these probe results do not explain
that fault. See [REM_OPENSSH_COMPATIBILITY.md](docs/REM_OPENSSH_COMPATIBILITY.md).

## Handoff / next executable work

- **Kernel compatibility owner:** correct Linux exit-status encoding; assess/add `mknodat` (33), `msync` (227), POSIX timer calls (107/409), and verify clone TLS argument forwarding.
- **Libc/runtime owner:** trace why public `clone(SIGCHLD)` returns `ENOSYS`; validate the full clone API and pthread creation after kernel fixes.
- **Boot/rootfs integration owner:** establish a session and controlling terminal for the login shell; provision `/dev/ptmx` and mount devpts consistently.
- **Retest owner:** rerun these three probes on the exact new QEMU/kernel/rootfs tuple after compatibility changes. Keep all changes in disposable test images until separately integrated.

## Required status fields

PASS:
- Standalone probes were built and executed for process/session calls, terminal/PTY operations, mappings, signals, timers, and pthread creation.
- Results and architectural implications are recorded in `docs/REM_ARCHITECTURE64.md`.

FAIL:
- Incorrect `_exit(24)` wait status; missing `mknodat`, `msync`, and POSIX timer calls; public libc clone `ENOSYS`; pthread creation `EAGAIN`.

BLOCKER:
- Runtime owner fixes and a subsequent run on an integrated tuple are needed to close the syscall and pthread findings.

FIXES MADE:
- Added three focused standalone probe sources and a concise, evidence-based architecture note. No runtime or production artifacts changed.

NEXT ACTION:
- Kernel/libc and boot/rootfs owners address the findings above; then rerun the probes against their exact integrated tuple.
