# MyEmulator2 Linux progress

## Current checkpoint

The GCC soft-float comparison ICE is fixed in commit `f3b76de`; signed
immediate lowering and its four-level regression are in `e313b76`.  GCC
soft-fp `libgcc` support is now generated and installed by `38ccfa0`.
The backend lowers DFmode comparisons through the appropriate libgcc
comparison helper and then uses the normal MyEmulator2 integer branch
patterns. The focused comparison fixture and the GCC C suite pass at
`-O0`, `-O1`, `-O2`, and `-Os`; `make spec-test cpu32-test` also passes.

## Current blocker

The pinned musl 1.2.5 build is being resumed from the disposable tree
`/tmp/myemu-musl-build10`, using source cache
`/tmp/myemu-musl-cache10` and prefix `/tmp/myemu-musl-install10`.
The build has advanced through complex math and is currently exposing
additional Linux generic syscall constants needed by musl's complete
source set. These constants are being added to the tracked MyEmulator2
musl syscall header; musl is not yet a completed or runtime-verified
libc.  The complete static archive and development headers now build when
the disposable configuration suppresses a target-wchar signedness warning.
The first static musl link succeeds with GCC soft-fp helpers.  A C `main`
linked against the static musl archive and the existing minimal Linux entry
object prints `MUSL C LIBC OK` under QEMU; Linux then reports the expected
PID-1 exit panic.  The full musl `crt1` startup remains unverified: its
initial short run exposed a truncated user stack in the musl `crt_arch.h`
logical-`andi` alignment sequence, which is fixed in the working tree, but
the larger startup still needs a bounded runtime diagnosis.  BusyBox is not
yet started.

Reproduction:

```sh
cp toolchain/musl/myemulator2/bits/syscall.h.in \
  /tmp/myemu-musl-cache10/musl-1.2.5/arch/myemulator2/bits/syscall.h.in
MYEMU_SOURCE_CACHE=/tmp/myemu-musl-cache10 \
MYEMU_MUSL_BUILD=/tmp/myemu-musl-build10 \
MYEMU_MUSL_PREFIX=/tmp/myemu-musl-install10 JOBS=1 \
  ./toolchain/scripts/build-musl.sh
```

The musl architecture overlay now includes `setjmp`/`longjmp` assembly
which preserves R5-R12, SP, LR, and TP.  The static archive was rebuilt
strictly and installed at `/tmp/myemu-musl-install10`.  BusyBox 1.37.0
was cross-compiled from the disposable source tree
`/tmp/myemu-phase3/busybox-1.37.0` using a minimal applet configuration;
the resulting file `/tmp/myemu-busybox-build11/busybox` is a valid static
ELF32 MyEmulator2 executable with entry point `0x00500000` and no
undefined symbols.

The BusyBox runtime is not yet verified.  A kernel rebuild with an
initramfs containing `/init` and BusyBox reached `Run /init as init
process` and loaded `/bin/busybox`.  A 90-second no-icount run then
ended with cause 14 while the host timeout terminated QEMU; cause 14 is
the architectural NMI, so this is harness-induced rather than evidence
of a BusyBox page fault.  A longer bounded run remains dominated by the
same very slow emulation and has not reached a shell prompt.  The next
diagnostic must distinguish emulator performance from a loader/startup
stall.  The temporary boot input is `/tmp/myemu-initramfs/list`; it must
not be committed.

Reproduction of the verified build milestone:

```sh
MYEMU_SOURCE_CACHE=/tmp/myemu-musl-cache10 \
MYEMU_MUSL_BUILD=/tmp/myemu-musl-build10 \
MYEMU_MUSL_PREFIX=/tmp/myemu-musl-install10 JOBS=1 \
  ./toolchain/scripts/build-musl.sh

MYEMU_MUSL_PREFIX=/tmp/myemu-musl-install10 \
MYEMU_TARGET_GCC="$PWD/.toolchain-install/bin/myemulator2-elf-gcc" \
  make -C /tmp/myemu-phase3/busybox-1.37.0 \
  O=/tmp/myemu-busybox-build11 \
  CC="$PWD/toolchain/scripts/myemulator2-musl-gcc" \
  CROSS_COMPILE="$PWD/.toolchain-install/bin/myemulator2-elf-" \
  CONFIG_EXTRA_LDLIBS='-lc -lgcc' \
  LDFLAGS='-static -L/tmp/myemu-musl-install10/lib -Wl,-Ttext=0x00500000' \
  SKIP_STRIP=y
```

## Storage bring-up status

A first polling block controller is now implemented at MMIO base
`0xf0100000`, backed by the QEMU block layer through the named backend
`myemulator2-disk`.  It exposes a 512-byte sector interface with
identify, capacity, read, write, range, read-only, and error status
registers.  The Linux side has a synchronous blk-mq driver and the
early page tables map the controller before `mm` exists.  QEMU and
Linux both build, and Linux discovers the disposable 1 MiB backend as
major 259, minor 0.

The first probe found two real defects: the early kernel page tables
did not map the block MMIO page, and QEMU had not acquired write
permission on the backend.  Both are fixed.  A manual instrumented run
then submitted a sector-1 write and read and showed the expected first
byte and persisted the pattern in the disposable image.  The tracked
end-to-end userspace test is not yet a passing regression: in the clean
reproducible run the userspace operation currently reports
`BLOCK DATA FAIL` without a corresponding device request, indicating a
remaining Linux block-device/cache or initramfs integration issue.
Storage, ext4, and disk boot must therefore remain unclaimed until
that test passes.

The immediate next task is to isolate that userspace block operation,
then return to the BusyBox startup/runtime timeout and add the
reproducible BusyBox configuration and integration test.

## Block userspace regression resolution

The userspace block mismatch was caused by the minilibc syscall
wrappers violating the frozen ABI.  `myemu_openat()`, `myemu_fstatat()`,
`myemu_renameat()`, `myemu_mmap()`, `myemu_clone()`, and `myemu_wait4()`
used R5-R7 as argument-shuffling temporaries without preserving the
callee-saved registers.  The block fixture kept its 512-byte loop count
in R5; after `openat()` the later write was called with a zero count,
and the read buffer consequently remained zero.  The wrappers now save
and restore every callee-saved register they use, including adjusted
incoming stack-argument offsets for mmap and clone.

The clean tracked block regression now passes, including persistence of
the written sector in its disposable raw image:

```sh
python3 toolchain/scripts/test-linux-block.py
```

The existing process, TTY, minilibc, and minilibc-shell regressions also
pass after the wrapper fix.  The next task is to add the reproducible
BusyBox build/runtime harness and diagnose its slow startup without
claiming an interactive BusyBox shell before it is observed.

## Reproducible BusyBox build

The tracked scripts now provide a pinned BusyBox 1.37.0 build and
initramfs assembly:

```sh
MYEMU_SOURCE_CACHE=/tmp/myemulator2-sources \
MYEMU_BUSYBOX_SOURCE=/tmp/myemu-phase3/busybox-1.37.0 \
MYEMU_BUSYBOX_BUILD=/tmp/myemu-busybox-build21 \
MYEMU_MUSL_PREFIX=/tmp/myemu-musl-install10 \
MYEMU_TARGET_GCC="$PWD/.toolchain-install/bin/myemulator2-elf-gcc" \
  ./toolchain/scripts/build-busybox.sh
MYEMU_BUSYBOX_BINARY=/tmp/myemu-busybox-build21/busybox \
  ./toolchain/scripts/build-busybox-initramfs.sh /tmp/myemu-busybox-initramfs21
```

The source archive is checksum-verified.  The builder selects the
required shell, filesystem, process, diagnostic, and vi applets and
does not depend on a generated source-tree configuration.  The static
BusyBox ELF and initramfs build successfully.  A bounded QEMU boot with
that initramfs reaches `/bin/busybox` but has not produced a shell prompt;
at the timeout boundary QEMU termination injects cause 14.  This remains
`TIMEOUT`, not a BusyBox runtime pass.

## Ext4 disk boot status

The disposable ext4 experiment reached a real disk boot. With the
initramfs disabled, Linux discovered the 16 MiB `myemu0` device, mounted
the ext4 filesystem read-write, reported `VFS: Mounted root (ext4
filesystem) on device 259:0`, and executed `/sbin/init` from that mounted
root. The init process then launched `/bin/busybox` and hit the same
unresolved BusyBox startup timeout before printing a shell message. Thus
ext4 discovery, mounting, and init lookup are observed, but file
persistence and an interactive disk-based shell are not verified.

Reproduction helpers are tracked:

```sh
MYEMU_BUSYBOX_BINARY=/tmp/myemu-busybox-build21/busybox \
  ./toolchain/scripts/build-ext4-rootfs.sh /tmp/myemu-ext4-test21.img
MYEMU_EXT4_IMAGE=/tmp/myemu-ext4-test21.img \
  ./toolchain/scripts/run-linux-ext4.sh
```

A separate guest-level persistence regression is now verified. PID 1
creates `/root/myemu-persist.txt`, writes a known pattern, calls the real
Linux `fsync` syscall, and exits. QEMU is then terminated and restarted
with the same disposable image; the second PID 1 reads and compares the
file from ext4 and reports `PERSIST PASS`:

```sh
python3 toolchain/scripts/test-linux-ext4-persistence.py
```

This verifies guest-created file persistence independently of BusyBox.

## BusyBox exec/page-table follow-up

Commit `60e7a82` balances the MyEmulator2 architecture's extra per-mm
user PTE page with `mm_inc_nr_ptes()`.  Before this fix, replacing the
initial process image produced `BUG: non-zero pgtables_bytes on freeing
mm: -4096`; the existing Linux process regression and the two-boot ext4
persistence regression both pass with the fix.

A focused direct `execve("/bin/busybox", ...)` fixture reaches Linux's
ELF exec path, but BusyBox startup remains unverified.  After the page
table accounting fix, the first genuine remaining failure is an irq-work
list traversal with a node pointer `0x01ed0000`:

```text
MyEmulator2 kernel fault: ... cause=6 info=01ed0000 ...
```

This is not the timeout harness's NMI.  It occurs while
`irq_work_run_list()` dereferences the queued node after `/init` has
called `execve()`.  The temporary linker/per-CPU experiment did not
change the pointer and was not retained.  The next action is to trace
the first `irq_work_queue()` producer and the allocator/per-CPU address
that supplies `0x01ed0000`, then rerun the direct BusyBox `true`
fixture before attempting the interactive shell.

## BusyBox R15 corruption fix

The first corruption was traced to the MyEmulator2 musl `setjmp`/`longjmp`
overlay, not to BusyBox or QEMU. The overlay initially used `r15` as a
temporary while saving TP, so `setjmp()` returned with GCC's fixed frame
pointer holding the TLS pointer. The corresponding `longjmp()` path also
needed independent `r15` and TP slots. This produced values such as
`r15=0x0076db4c`, corrupting subsequent stack frames and eventually causing
a NULL dereference in `procargs()`.

The overlay stores preserved `r15` at offset 40 and TP at offset 44, and
`longjmp()` restores both independently. `setjmp()` now uses caller-saved
`r3` for the TP temporary so it returns with `r15` unchanged. Rebuilt musl
and BusyBox reach `ash_main()` with a valid argv pointer; the earlier NULL
`procargs()` fault is no longer present. The direct `true` fixture exits via
the normal PID1 path with status 0. The interactive shell still needs a
completed socket-backed terminal test.

## BusyBox `.bss` follow-up

The next failure was not another register corruption.  On the second
`execve()` (the BusyBox image), the anonymous page containing musl's
`__malloc_context` was recycled with stale list pointers.  `ash` then
entered `mallocng` with a non-zero active-list head and eventually stored
through a null list link.  The architecture page-fault path now explicitly
zeros a newly installed anonymous page, including pages first touched by a
supervisor `copy_from_user()` during ELF loading.  A direct `busybox sh -c`
fixture then reached `SHELL_READY`, and the existing process, TTY and hosted
C-shell regressions still pass.

The remaining BusyBox blocker is the full interactive init path.  The
serial console is now registered as `ttyMY0` and the initial-console warning
is gone, but the shell has not yet produced a prompt or echoed input through
the socket-backed test.  Do not classify this as an interactive-shell pass.

## BusyBox shell performance diagnosis

The socket-backed BusyBox shell test initially appeared to hang, but QMP
register inspection showed the guest executing Linux `__const_udelay()` rather
than remaining in BusyBox. The architecture delay primitive now uses an
explicit `subi 1` loop instead of a C post-decrement. The process regression
remains passing; BusyBox shell completion is still not verified and requires
further performance/runtime investigation.
