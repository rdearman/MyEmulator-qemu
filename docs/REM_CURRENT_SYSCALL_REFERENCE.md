# Current MyEmulator2 syscall reference

Generated from `linux/arch/myemulator2/kernel/syscall.c`. This records implemented dispatcher cases; it is not a claim of complete Linux ABI coverage. Tested coverage is a separate fact and is recorded by the named regression scripts.

The userspace convention is `r1` = number/result and `r2`–`r7` = arguments. The `syscall` immediate is a diagnostic field; current Linux handling ignores it, so examples use `syscall 0`.

| Number | Name | Kernel status |
|---:|---|---|
| 17 | `getcwd` | dispatcher case |
| 21 | `access` | dispatcher case |
| 23 | `dup` | dispatcher case |
| 24 | `dup3` | dispatcher case |
| 25 | `fcntl` | dispatcher case |
| 29 | `ioctl` | dispatcher case |
| 34 | `mkdirat` | dispatcher case |
| 35 | `unlinkat` | dispatcher case |
| 38 | `renameat` | dispatcher case |
| 46 | `ftruncate` | dispatcher case |
| 49 | `chdir` | dispatcher case |
| 50 | `fchdir` | dispatcher case |
| 52 | `fchmod` | dispatcher case |
| 53 | `fchmodat` | dispatcher case |
| 56 | `openat` | dispatcher case |
| 57 | `close` | dispatcher case |
| 59 | `pipe2` | dispatcher case |
| 61 | `getdents64` | dispatcher case |
| 62 | `lseek` | dispatcher case |
| 63 | `read` | dispatcher case |
| 64 | `write` | dispatcher case |
| 65 | `readv` | dispatcher case |
| 66 | `writev` | dispatcher case |
| 72 | `pselect6` | dispatcher case |
| 73 | `ppoll` | dispatcher case |
| 74 | `fsync` | dispatcher case |
| 78 | `readlinkat` | dispatcher case |
| 79 | `fstatat` | dispatcher case |
| 80 | `fstat` | dispatcher case |
| 81 | `sync` | dispatcher case |
| 83 | `fdatasync` | dispatcher case |
| 88 | `utimensat` | dispatcher case |
| 93 | `exit` | dispatcher case |
| 94 | `exit_group` | dispatcher case |
| 96 | `set_tid_address` | dispatcher case |
| 99 | `set_robust_list` | dispatcher case |
| 101 | `nanosleep` | dispatcher case |
| 124 | `sched_yield` | dispatcher case |
| 129 | `kill` | dispatcher case |
| 130 | `tkill` | dispatcher case |
| 131 | `tgkill` | dispatcher case |
| 132 | `sigaltstack` | dispatcher case |
| 134 | `rt_sigaction` | dispatcher case |
| 135 | `rt_sigprocmask` | dispatcher case |
| 144 | `setgid` | dispatcher case |
| 146 | `setuid` | dispatcher case |
| 154 | `setpgid` | dispatcher case |
| 155 | `getpgid` | dispatcher case |
| 156 | `getsid` | dispatcher case |
| 157 | `setsid` | dispatcher case |
| 160 | `uname` | dispatcher case |
| 166 | `umask` | dispatcher case |
| 172 | `getpid` | dispatcher case |
| 173 | `getppid` | dispatcher case |
| 174 | `getuid` | dispatcher case |
| 175 | `geteuid` | dispatcher case |
| 176 | `getgid` | dispatcher case |
| 177 | `getegid` | dispatcher case |
| 178 | `gettid` | dispatcher case |
| 214 | `brk` | dispatcher case |
| 215 | `munmap` | dispatcher case |
| 220 | `clone` | dispatcher case |
| 221 | `execve` | dispatcher case |
| 222 | `mmap` | dispatcher case |
| 226 | `mprotect` | dispatcher case |
| 260 | `wait4` | dispatcher case |
| 261 | `prlimit64` | dispatcher case |
| 278 | `getrandom` | dispatcher case |
| 291 | `statx` | dispatcher case |
| 403 | `clock_gettime64` | dispatcher case |
| 422 | `futex` | dispatcher case |

Regenerate with:

```sh
python3 tools/generate-myemulator2-syscall-reference.py
```
