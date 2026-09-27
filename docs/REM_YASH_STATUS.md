# REM Yash status

Yash reaches its prompt after the ELF/BSS loader issue was corrected. Its line
editor waits for input with `pselect()` in `sig.c:wait_for_input()` (the
`pselect(fd + 1, ...)` call at line 677), and musl lowers that call to the
32-bit generic Linux `pselect6` syscall, number 72. The historical disposable
kernel used for the failure implemented `ppoll` (73) but had no case for 72,
so it returned `-ENOSYS`; Yash reported this as “Function not implemented”
while waiting for input.

The disposable kernel now dispatches syscall 72 to
`sys_pselect6_time32()`, preserving the Linux fd-set, timeout, and signal-mask
semantics. The development-tree dispatcher already contains the equivalent
case; the failure came from testing an older kernel build. This is a kernel
support fix, not a Yash workaround. Diagnostic
entry/return logging is restricted to processes whose command name is `yash`.

Earlier disposable build (before syscall 133 support):

```text
/tmp/rem-winsize-clean/build/vmlinux
SHA256 f2c3a331e96745dd841c485fa33979225c8ba9f7d3f2b7f18afdb1533dad4213
```

The flight kernel and rootfs are unchanged. The later kernel listed below
contains the syscall 133 change.

```text
Yash   ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e
Kernel f2c3a331e96745dd841c485fa33979225c8ba9f7d3f2b7f18afdb1533dad4213 (historical)
QEMU   0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a
Rootfs 939b7554c7030e21ded162ec8fd41d25b4dedd1ca07b71072db8c9b9d224c2b7 (historical)
```

The disposable environment test then exposed syscall **133**. In the generic
32-bit Linux ABI this number is `rt_sigsuspend` (the old short-name
`sigsuspend` is not the correct dispatcher entry). The disposable
`myemulator2` syscall table now routes 133 to Linux's `sys_rt_sigsuspend`,
which applies a temporary signal mask, sleeps until signal delivery, and
restores the caller's mask through the normal kernel implementation.

With the corrected kernel and a clean disposable rootfs, a Yash command
script using `PATH=/bin:/usr/bin:/sbin:/usr/sbin`, `HOME=/root`, and
`TERM=xterm` produced `hello` and exited with status 0. This confirms the
previous ENOSYS boundary is fixed. A complete interactive transcript from
the parent shell has not yet been captured, so final Yash acceptance remains
**PARTIAL**, not PASS.

Disposable sigsuspend kernel:

```text
/tmp/rem-winsize-clean/build/vmlinux
SHA256 9ca1141421cab0142a9f78c79035b8c0cb13762a8fb166a4aada6e8ef90507fb
```

Disposable validation rootfs:

```text
/tmp/rem-yash-clean/rootfs-sigsuspend2.ext4
SHA256 be0a377afe775d1251b39bd04d18a61345fec41473847d691c9ecc28b294aaa1
QEMU SHA256 0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a
Yash SHA256 ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e
```

The syscall change is confined to
`/tmp/rem-winsize-clean/linux/arch/myemulator2/kernel/syscall.c`; the flight
kernel and rootfs are unchanged.

## Flight-image staging

The validated Yash binary was staged outside the production image at:

```text
/tmp/rem-yash-flight-stage/usr/bin/yash
SHA256 ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2a8e
Source revision ad34d458ed2b21b383e84234c7d3d58a654dfb4a
```

The accompanying disposable profile is
`/tmp/rem-yash-flight-stage/etc/profile` and exports `PATH`, `HOME`, and
`TERM` for Yash launches. The required kernel dependencies are `pselect6`
(syscall 72) and `rt_sigsuspend` (syscall 133).

The production `REM-FLIGHT-TTY/rootfs/rem-rootfs.ext4` was not modified.
The disposable test reached `REM#` and showed the entered
`/usr/bin/yash` text, but the current parent-shell/serial command-delivery
path did not execute the command, so the required flight-image launch test
has not passed. Yash is therefore **STAGED, NOT INTEGRATED** pending a
working disposable parent-shell command test.

## Combined Yash/Kilo staging

Both validated tools are also staged together in a disposable developer
image:

```text
/tmp/rem-dev-flight-stage/usr/bin/yash
SHA256 ca2d37be818cb54ac67888f3fec0afb02f0c8d367ccc5cc38d12c89eb07a2e8

/tmp/rem-dev-flight-stage/usr/bin/kilo
SHA256 a8495a80effb8168116ff123eb0fd470ddfa357642b6320cd7eb07d95474b30f

/tmp/rem-dev-flight-stage/rootfs.ext4
SHA256 197a57651fb29d8b767523ae4f00d922066fa109add15f7506aa1aaf49c8b8a8
```

Kilo is the antirez implementation (`KILO_VERSION 0.0.1`) from
`/tmp/rem-kilo-upstream/kilo.c`; the local source snapshot has no revision
metadata. Its independent edit/save/reopen and reboot persistence test
passed. The combined image has not been promoted to `REM-FLIGHT-TTY` because
the required parent-shell Yash launch test remains blocked by command
delivery.

## Unified disposable flight candidate (2026-09-27)

The command-delivery issue above is superseded by the following isolated
QEMU test. No production image was changed.

```text
Kernel /tmp/rem-yash-unified-build/vmlinux
SHA256 5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a

Rootfs /tmp/rem-yash-qual/flight-yash.ext4
SHA256 8649389a39aefa45d9a1ed0b2b8438cde5396cbb1fe1991ee26d4633cff48d59

QEMU REM-FLIGHT-TTY/qemu/desktop/qemu-system-myemulator32
SHA256 0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a
```

The kernel is built from the Yash-capable kernel baseline with the current
syscall dispatcher, including `faccessat` (48), `faccessat2` (439), and the
Yash-required `rt_sigsuspend` (133). On this tuple Yash booted, presented its
interactive prompt, executed a command, and passed a real cursor-edit test.
The staged chibicc, native `as`/`ld`, CRT, musl libc, and `libgcc.a` hashes
match the native hosted-C tuple in `REM_VALIDATED_RUNTIME_TUPLES.md`.

The Yash-driven compile chain is not yet qualified: the chibicc child was
reported killed by `SIGHUP`, so no executable was produced. Kilo was not
retested on this newly combined tuple.

Included in this rootfs: Yash 2.61, Kilo, chibicc, native `as`/`ld`, CRT
objects, musl `libc.a`, and REM `libgcc.a` (tool hashes match the native
hosted-C tuple above). The previously staged `samu` binary is not qualified
for this tuple. Missing qualification: Yash-driven compile/link/execute,
Kilo handoff on this tuple, and a current Samurai binary from Codex #2.
