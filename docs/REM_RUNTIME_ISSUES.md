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
