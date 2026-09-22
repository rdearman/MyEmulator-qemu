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
## BusyBox interactive tty milestone

The real serial-core tty path is now verified for the initramfs system.  The
init handoff explicitly redirects BusyBox ash through `/dev/console`, because
PID 1 starts without inherited descriptors on this architecture port.  The
Linux syscall dispatcher now includes the `fcntl`, `writev`, `getppid`, and
`geteuid` operations needed by ash startup and terminal output.

Reproduce the verified shell test with:

```sh
MYEMU_BUSYBOX_BINARY=/tmp/myemu-busybox-build23/busybox \
  ./toolchain/scripts/build-busybox-initramfs.sh /tmp/myemu-busybox-shell-ttyfix
.linux-build/linux-6.12.1/scripts/config --file .linux-build/build/.config \
  --set-str INITRAMFS_SOURCE /tmp/myemu-busybox-shell-ttyfix/initramfs.cpio
./toolchain/scripts/build-linux.sh
MYEMU_SHELL_PROMPT_TIMEOUT=20 python3 toolchain/scripts/test-linux-busybox-shell.py
```

This passes the prompt test, terminal input/output, redirection and twenty
consecutive shell commands.

The ext4 root image now also reaches the interactive shell.  The blocker was
the architecture page-fault path failing to handle `VM_FAULT_RETRY`: file
backed faults can drop `mmap_lock` while the block layer supplies an ELF page,
so the unconditional unlock corrupted the rwsem during the second `execve()`.
The handler now reacquires the lock and retries once without
`FAULT_FLAG_ALLOW_RETRY`.  The ext4 shell regression reaches the prompt,
creates and reads a file, and executes twenty commands.

## Native Linux GCC bring-up

The hosted GCC build now supports the `myemulator2-linux-musl` target. The
target-specific libgcc configuration excludes unsupported DWARF exception
unwinding, includes the MyEmulator2 libgcc fragment, and handles all GCC
soft-float comparison predicates. A separate hosted target header supplies
musl startup and libc link specs while preserving the freestanding compiler
target.

The compiler installs the user linker script into the Linux sysroot so normal
invocations produce the same 0x00700000 user image layout as the existing
musl programs. `python3 toolchain/scripts/test-linux-native-gcc.py` compiles
a C program with the staged compiler, boots it as `/init` in a disposable
initramfs, and verifies `native-gcc-pass` on the QEMU serial console. This is
the first verified native-toolchain runtime milestone; native binutils and
GNU Make are not yet installed or verified inside the guest.

## Hosted GNU Make follow-up

The hosted libgcc configuration is now reproducible for the Linux target:
`toolchain/gcc/t-myemulator2-linux` includes the soft-float support required
by musl applications while omitting unsupported exception-unwind objects.
The source-preparation script installs that fragment in `libgcc/config`,
where GCC's libgcc build actually consumes it. With the staged native GCC,
binutils and musl installation, GNU Make 4.4.1 compiles and links as a
static MyEmulator2 ELF32 executable.

The guest Make regression is not yet verified. The corrected test harness now
embeds its own initramfs, and the process and native-GCC fixtures pass with
that correction. GNU Make reaches userspace without an exception, but does
not emit its `native-make-pass` marker within the bounded 45-second test.
Tracing identified and fixed two prerequisites: R15 must be materialized at
the syscall/TB boundary, and musl's futex, set_tid_address, gettid,
rt_sigaction and rt_sigprocmask calls must reach Linux instead of returning
`-ENOSYS`. The remaining Make-specific hang is still under investigation.

## Native GCC MMU regression fix

The native-GCC startup regression was traced to the per-process copy of
the kernel's low identity page tables. Linux user executables are linked
at `0x00700000`, but `pgd_alloc()` previously removed only the stale
`0x00500000` PTE. The remaining supervisor-only identity entries covered
the 7--8 MiB user image window. During ELF `padzero()` and other supervisor
accesses to user addresses, those entries resolved to physical identity
memory instead of the user mapping, leaving the final TLS/BSS page corrupt.
The resulting bad pointer caused repeated faults in musl startup.

`pgd_alloc()` now clears the 7--8 MiB PTE range in each user page table while
retaining the lower kernel identity mappings. Demand faults then install the
actual user PTE before supervisor accesses are retried. Separately, QEMU
reloads R1--R12 at translation-block boundaries after exception/syscall
return; R0 remains hardwired and R13/R15 retain their existing targeted
handling. This is required because Linux writes the saved return registers
while the exception path does not return through the interrupted TB.

The investigation command is:

```sh
python3 toolchain/scripts/test-linux-native-gcc.py
python3 toolchain/scripts/test-linux-process.py
```

An intermediate run passed native GCC after the page-table change, but the
current clean rerun still fails during the supervisor `padzero()` access at
`0x00702754`, before the user program can start. The fix is therefore not
yet committed or classified as verified; the next session must instrument
the supervisor page-fault return and confirm that the user VMA mapping is
installed before accepting this change. Native GNU Make remains blocked.

## Native GCC follow-up (2026-09-21)

The rebuilt hosted compiler and the supervisor page-fault/QEMU stack-state
work in `0e77f38` now pass the native GCC guest regression. The last fixture
failure was environmental: PID 1 had no inherited standard descriptors while
musl stdio selected `writev(2)`. The dispatcher now provides the same
`/dev/console` fallback for `writev` as for `write`, and the fixture explicitly
opens and duplicates `/dev/console` before printing.

Verified:

```sh
make spec-test
make cpu32-test
python3 toolchain/scripts/test-linux-process.py
MYEMU_NATIVE_GCC=/tmp/myemu-native-new/bin/myemulator2-linux-musl-gcc \
  python3 toolchain/scripts/test-linux-native-gcc.py
```

Native GCC prints `native-gcc-pass`. Native GNU Make remains unverified: it
reaches mmap/heap startup but does not emit `native-make-pass` within a bounded
180-second diagnostic run. The last captured state is repeated user loads
against zero-page mappings near `0x1000...`; this is the next MMU/page-fault
investigation, not a Make acceptance result.

## Native GNU Make page-fault investigation (2026-09-21)

The bounded Make fixture was instrumented at the QEMU TLB boundary. The
reported `0x1000...` accesses are in GNU Make's `hash_find_slot()` (for
example PC `0x007111cc`), and QEMU reads the corresponding present PDE/PTE
with valid user read permissions. The `0x0000000f` PTEs were private writable
VMA zero-page mappings, not failed translations. The architecture now keeps
private writable VMAs read-only until a write fault (`PAGE_COPY` is also
read-only), preventing a read fault from exposing physical frame zero as
writable. The process, minilibc and native-GCC regressions still pass after
this change.

The remaining Make result is `TIMEOUT`, not a pass: a 60-second run reaches
valid heap pages and a legitimate stack-growth fault but does not emit
`native-make-pass`. This is currently a severe guest-startup/emulation
performance issue or a later Make/libc defect; no GNU Make acceptance claim
has been made. The native Make harness now uses a parent-death signal,
separate process groups and a configurable bounded timeout so interrupted
host runs do not leave QEMU children behind.

## Self-hosting and deployment status (2026-09-21)

The current `test-linux-native-gcc.py` fixture must not be described as
guest-native compilation. Its `MYEMU_NATIVE_GCC` executable is an x86-64
Linux host driver which invokes the REM cross compiler to produce a guest
ELF, and that ELF is then executed under QEMU. `file` reports the driver as
x86-64, not MyEmulator2/REM. A genuine self-hosted milestone still requires
a Canadian-cross build whose GCC, assembler and linker executables are
themselves REM Linux ELFs, followed by compilation entirely inside the guest.
The current native GNU Make fixture remains `TIMEOUT`, so the self-hosted
toolchain, GDB, applications and Android package are not yet verified.

The Canadian-cross binutils investigation began from the existing x86-64
host driver. The first attempt exposed malformed BFD source lists caused by
over-broad `cpu-moxie`/`elf32-moxie` substitutions in
`prepare-binutils-source.py`; commit `5825a88` narrows those substitutions
to exact `.c` and `.lo` names and adds Linux target triples to BFD target
selection. A fresh source tree now configures cleanly, but the guest-hosted
build currently stops in the recursive `ld` configure because it cannot create
`sub/conftest.c`. This remains an implementation blocker for the Canadian
cross and is not a verified native-toolchain result.

The fresh Canadian-cross retry reaches the recursive `ld` configure, but
that configure's dependency-mode probe fails before compiling its test
source (`cc1: conftest.c: No such file or directory` / `sub/conftest.c`).
The generated BFD lists are now correct; the remaining work is to make the
binutils build system's host/target configure recursion work with the REM
host compiler, then build and install the guest binutils before bootstrapping
guest GCC.

## Kernel/user address-space alias fix (2026-09-22)

The first writer of the corrupted SLUB freelist was traced to an address-space
alias, not CAS32.  The kernel identity map covers physical addresses 0--16
MiB, while the old userspace image at `0x00700000` occupied the same virtual
window.  Under a user PTBR, virtual page `0x0084f000` therefore resolved both
to its identity physical page and to an unrelated userspace page; a supervisor
SLUB access read `0x05ad0ffc` from the latter mapping.  The allocator fault at
`0x05ad100c` was only the first visible consumer.

The Linux userspace image base is now `0x02000000`, above the identity map.
New per-process user PDE/PTE storage starts empty instead of copying the old
identity PTE page, and low-linked Linux fixtures use the same non-overlapping
base.  The temporary allocator and MMU traces were removed.  `make spec-test`,
`make cpu32-test` and `python3 toolchain/scripts/test-linux-process.py` pass
with the rebuilt QEMU and kernel.  Genuine guest-native GCC compilation is
still not accepted: the existing self-hosted fixture currently stalls in a
later boot/console or block-I/O path and needs separate diagnosis.

## Private low PTEs and delay-loop safety (2026-09-22)

The shared-PTE alias was corrected by giving each process private copies of
the four low identity-map PTE pages, with supervisor-only PDE flags. The
architecture now also frees the direct PGD-resident PTE pages during
`pgd_free`, preventing low user faults from rewriting kernel identity
mappings and avoiding leaks across `execve`/exit. A zero guard was added to
the optimized `__delay` loop; without it, a sub-256-unit delay shifted to
zero and decremented to `0xffffffff`, creating an apparent kernel hang.

After rebuilding the kernel and clean QEMU, `make spec-test`, `make
cpu32-test`, `python3 toolchain/scripts/test-linux-process.py`, and
`python3 toolchain/scripts/test-linux-ext4-persistence.py` pass. The genuine
guest-native GCC fixture still does not reach its marker: after the native
compiler artifacts were relinked at `0x02000000`, the guest enters an
NMI/panic path during the fork/exec test. The exact NMI injection source
remains open; no QEMU process is left running by the test harness.
