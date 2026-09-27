# Assembly syscall cookbook

All examples use the current MyEmulator2 GAS convention: put the syscall
number in `r1`, arguments in `r2`–`r7`, execute `syscall 0`, and find the
result in `r1`. A negative result is a kernel `-errno`; preserve any live
caller state before making the call. Numbers are generated in
`REM_CURRENT_SYSCALL_REFERENCE.md`.

## Scalar call: close or exit

```asm
li   r1, 57          # close
li   r2, 3           # fd
syscall 0
bltz r1, .error
```

For process termination use number 94 (`exit_group`) and status in `r2`.

## Pointer and length: write/read

```asm
li   r1, 64          # write
li   r2, 1           # fd
la   r3, message
li   r4, message_end-message
syscall 0
```

`read` is number 63 with the same fd, buffer, count pattern. Reads and writes
may be short; loop on the remaining count. Retry on `-EINTR`, and only retry
`-EAGAIN` when the file descriptor is intentionally nonblocking.

## Pathname: openat

Number 56 takes directory fd, pathname pointer, flags and mode in `r2`–`r5`.
The pathname must be NUL-terminated and remain valid for the call. Check the
negative result before using it as a file descriptor.

## Pointer to a structure: ioctl

Number 29 takes fd, request number and structure pointer. Structure layout and
request constants belong to the target headers; do not guess offsets in an
assembly-only program. This is why the worked terminal bridge keeps termios
layout in a small C helper.

## Waiting: ppoll

Number 73 takes a `struct pollfd` array pointer, count, timeout pointer and
signal-mask pointer. The pointed-to structures must use the target's 32-bit
layout. See the POSIX/Linux reference and headers before writing them by hand.

## Processes: clone, execve, wait4

The dispatcher exposes clone (220), execve (221), and wait4 (260). These are
five-argument Linux interfaces and are best wrapped in a small assembly module
with a documented stack/child-entry contract. Do not substitute a normal
`fork` example: `fork` is not the interface exposed by this target.

## Memory: brk and mmap

`brk` is 214 and `mmap` is 222. Both return an address or a negative errno;
page alignment, flags and protection values come from the target headers.
Static musl/minilibc may choose different allocation paths, so inspect the
current libc source before replacing its allocator.
