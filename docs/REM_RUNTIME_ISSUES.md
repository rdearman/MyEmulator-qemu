# REM runtime issues

## Process groups/job control support required for Bash job control

**Status:** Open, tuple-specific compatibility gap. Bash 5.2.37 itself builds
from GNU source as a static REM ELF and passes the tested noninteractive
workloads, interactive input/output, and `fork`/`exec`/`waitpid` probes.

On the tested QEMU/kernel/rootfs tuple (`QEMU 0154db…`, kernel
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`,
rootfs `c049d90b5bc90307dbcd0f299ad37e08c3c843cc08d0ba91d18c2461c78dd7d7`),
`setpgid(0,0)` returned `-1`/`ENOSYS`; `tcsetpgrp(0,getpgrp())` returned
`-1`/`EINVAL`. Bash consequently reported that it could not set the terminal
process group and ran without job control. The kernel/libc cause of `ENOSYS`
has not been isolated. A later terminal/session probe on a different kernel
showed `setpgid` works and `tcsetpgrp` works after a controlling terminal is
acquired; see [REM_VIM_COMPATIBILITY.md](REM_VIM_COMPATIBILITY.md). Therefore
this issue records the older tuple's observed gap and does not assert that
current kernels universally lack these operations.

**Required behaviour:** POSIX interactive job control needs working process
groups and foreground-terminal process-group control, with appropriate
session and controlling-terminal setup. **Open:** identify why `setpgid`
returned `ENOSYS` on the recorded tuple and which integration layer owns it.

Evidence: [REM_BASH_COMPATIBILITY.md](REM_BASH_COMPATIBILITY.md), including
the process probe output and artifact provenance.

## OpenSSH source-transfer runtime gaps

**Status:** Open, reproduced on the disposable 2026-09-28 diagnostic tuple;
no production fix has been made. With the corrected OpenSSL header ordering,
the hash-staged OpenSSL case 4 (`OPENSSL_init_crypto(); RAND_status()`) passed.
OpenSSH 10.5p1 `ssh -V` also passed using static target binary SHA-256
`684db7866f88b41fd2cd2cc9fe2acb438e78cea353db42797c27a69ffd5ade05`.

Key generation with either RSA or Ed25519 failed at
`CRYPTO_THREAD_write_lock` (PC `0x02125c1c`, bad address `0x2d534544`), called
from `ossl_method_lock_store` (return PC `0x0214e084`). `OPENSSL_CONF=/dev/null`
did not alter the result. `sshd -t` faulted in `getaddrinfo` (PC
`0x023d3974`, address `0xfffffef4`, FP `0x570`); SSH connection setup also
faulted in the `getaddrinfo` path. SCP reported `socketpair: Function not
implemented` (ENOSYS), then its child SSH faulted. No host key, listener,
authenticated connection, or copied file was validated.

**Classification:** OpenSSL provider/method-store state and libc/kernel
`getaddrinfo`/`socketpair` paths; ownership of the faults is not yet isolated.
The exact binaries, hashes, and disposable tuple are in
[`REM_OPENSSH_COMPATIBILITY.md`](REM_OPENSSH_COMPATIBILITY.md). The standalone
clock probe returned success while leaving a sentinel nanoseconds field
unchanged; see the REM64 time-syscall note. The OpenSSL RAND readiness pass is
valid only for the corrected private build and seeded disposable QEMU profile.

## Static userspace frame-register corruption in `getaddrinfo`

**Status:** backend/ABI integration defect evidenced in a static rsync 3.5.1
binary; no production fix yet. On a c775-based disposable image, the URL-mode
rsync invocation faulted at `0x020bee54` with address `0xfffffef4`, while the
fault record showed valid SP `0x3fbc3290` and `r15/fp=0x570`. The disassembly
places the faulting store immediately after `getaddrinfo` returns from
`__lookup_serv`. That callee stores its frame size `0x570` at `4(r13)` and
reloads `r15` from that slot in its epilogue; the caller then uses the returned
`r15` as its frame base. This identifies generated userspace frame/register
preservation as the immediate failure mechanism. This matches the previously
documented old-GCC large-frame regression. A minimal numeric `getaddrinfo`
test independently failed with old musl archive hash
`30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924` and
passed at `-O0` and `-O2` with rebuilt archive
`312c6c393d9b879448edd28a822ec4fd56e1f40e364764c5da77a0df884ba21f`.
The strongest current explanation is stale statically linked libc/compiler
output; rsync has not yet been relinked with the newer archive, so this is a
root-cause classification rather than an end-to-end fix. No production artifact
was changed here. The fault is not evidence of filesystem corruption. Its
logged preceding syscall 221 is temporally adjacent, but syscall causality has
not been demonstrated.

A local `rsync -rtp` copy of a two-file fixture reproduced the same PC,
address, and `fp=0x570` before a copy result was available. This rules out a
remote daemon or external peer as a necessary trigger. Another local-copy run
logged `MYEMU llist bad add`, but did not complete; that kernel diagnostic is
still not causally tied to the frame-register fault.

The same bad address and `fp=0x570` were recorded in earlier `sshd` and `ssh`
`getaddrinfo` failures; matching the register signature makes a shared runtime
cause plausible, but those binaries still need instruction-level confirmation.
Separately, the tracked minimal cancellable-syscall probe's `/dev/zero` read
passed and its connect returned `ENETUNREACH`; it did not reproduce the reported
`__syscall_cp_c` SIGBUS. See [REM_OPENSSH_COMPATIBILITY.md](REM_OPENSSH_COMPATIBILITY.md)
for tuple records and [REM_ARCHITECTURE64.md](REM_ARCHITECTURE64.md) for the
disassembly evidence and architectural requirement.
