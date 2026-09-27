# REM32 ext4 persistence investigation

## Tested package tuple

All guest runs used the 2026-09-25 `REM-FLIGHT` desktop QEMU/kernel/rootfs
package, with a private writable-image copy under `/tmp/rem-ext4-codex7`:

| Component | Package path | SHA256 |
| --- | --- | --- |
| QEMU | `.rem-phone-deploy-20260925/REM-FLIGHT/qemu/qemu-system-myemulator32` | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` |
| Kernel | `.rem-phone-deploy-20260925/REM-FLIGHT/boot/vmlinux` | `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` |
| Rootfs | `.rem-phone-deploy-20260925/REM-FLIGHT/disk/rem-rootfs.ext4` | `46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` |

The 10 GiB seed passed read-only `e2fsck -fn` before testing. Reported initial
space was 2,432,649 of 2,621,440 blocks free (about 9.28 GiB) and 643,420 of
655,360 inodes free. No near-full or ENOSPC test was run.

## Persistence evidence

- Small text, source/header/assembly files, logs, and a 5,160-byte multi-block
  file survived guest reboots with matching SHA256 values. The multi-block
  file was written as 40 shell-builtin `echo` appends; every command and guest
  `sync` returned 0. Its recovered host hash was
  `ae6b9d02662f48cd2e3923710d768e0dcc2a5c26090599931bf6ae6be3dcdd10`.
- A 16,016-byte file written as sixteen 1,000-byte guest appends also survived
  reboot and host extraction with matching SHA256
  `7672803fd920d3f5a0d26ba2055f33a52283887397c49669dcf2c22df6742790`;
  guest commands, `sync`, and post-reboot hash all returned 0.
- A development-like tree (`.c`, `.h`, `.s`, object/executable *fixtures*, and
  build log) survived multiple boots. The restored copy booted and guest
  `sha256sum` matched all six files. The executable fixture retained mode
  0755 as reported by `debugfs` and on the extracted host copy. These fixtures
  test storage and mode persistence; they are not compiler outputs.
- A real 44-byte C source was recovered from an ext4 image on the host and
  matched byte-for-byte. SHA256:
  `7dd3fa764b3c55221f1b9dc441350c4e8bf0ed638ea39e49518c0bcb53041457`.
- The complete development fixture tree was recovered from the backup image;
  extracted source and fixture hashes matched the guest hashes. `debugfs`
  printed harmless ownership-change warnings because extraction ran as an
  unprivileged host user; file contents and mode were verified independently.

The guest has no working clean poweroff/remount-read-only path in this tuple:
`poweroff -f` reports `poweroff: applet not found`, and ext4 remount
read-only returns `Function not implemented`. Successful cycles used guest `sync`, exited the supervised
shell (PID 1 syncs), then stopped QEMU. This is synchronized shutdown followed
by journal recovery, not a proven clean unmount.

An immediate-SIGKILL characterization test wrote a distinctive file, observed
guest `echo` status 0, then killed QEMU without `sync`. The next boot logged
`EXT4-fs: recovery complete`, but the unsynced file was absent. This is a lost
unsynced write, not observed ext4 structural corruption. Do not rely on an
unclean stop to preserve recent work.

## Failure isolation

The larger-write fault reproduces without storage: `printf '%8192s' '' >
/dev/null` reaches a bad user fault (`pc=0x0200af7c`, address `0x2f4`, cause
7). Disassembly shows the faulting instruction loads from `r15+8164`; the
fault report has `r15=0x00000ad0`, outside the process's user VMA. With the
QEMU load decoder's signed 13-bit immediate, the instruction and saved base
imply effective address `0x00000ab4`, while the exception reports `info=0x2f4`.
Both are invalid user addresses, but the mismatch means fault-address/frame
reporting needs a separate QEMU/kernel audit. A 438-character
compound command containing four BusyBox `printf` calls reproduces the same
fault in each `printf` process; each exits 139 and the shell continues. The
current tuple did not panic on this reproducer. Earlier runs did record an
illegal-instruction panic after a user fault, so exception return remains a
separate unresolved failure path. A file-redirection run produced the same
user fault. Paired QEMU
`blk_co_pwritev` traces show the same 136 post-prompt 512-byte writes, with
identical sector offsets, in both runs; the last event is offset 454144. No
file-specific block request occurs before the exception. This reproducer
therefore points to the guest userspace/exception path, not ext4 or the MMIO
block driver. The kernel prints the PC and registers but no symbolized call
stack. A source snapshot at
`.rem-memory-scale-20260925/linux-6.12.1/lib/llist.c` contains the warning
`MYEMU llist bad add`. Its temporary guard rejects pointers outside
`0x00101000..0x01000000`, although the guest has 128 MiB RAM and the reported
`0x808b0000`/`0x8083c540` values are kernel virtual pointers. The warning is
therefore a false positive from the diagnostic range check, not evidence of
list corruption.

## Compiler workload persistence follow-up

On a disposable copy of the pinned 2026-09-27 chibicc candidate (same QEMU and
kernel hashes as the tuple above; rootfs SHA256
`c9f19fab0d27b7a96bec09bf09fc38f37059f7e462d2b9ca4e68bf57fb61c0de`), one
guest session created multiple C/header/assembly files, compiled an object,
and synced. After reboot and ext4 journal recovery, the guest hashes matched
the pre-reboot values; host extraction from a private recovered copy matched
the same hashes. A single-source chibicc executable also compiled and ran
before sync, then its source and executable hashes matched after reboot and
host extraction. The executable ran successfully twice when invoked alone
after reboot. One compound hash-and-run command returned `Hangup`/129 once;
separate retries passed.

Verified host-recovered SHA256 values: multi-source tree `a.c`
`2dc7b103bf1583d98946cb666c07b1f86dfe023e12fbf0868bde383935de356b`, header
`h.h` `c24a2ae9274b25129e49938b464003562c75daa22800a62f178c6eb10737dd89`,
`m.c` `4520ad2df4e5f17a252fdb682406de7dfba48b8cf866996c2501305a68f1bab3`,
and `m.o` `4848a216502ea633f6755066f9052d208f13bacd42ab5dcdca9304bf65142670`;
single-source executable `8f89379fe184da5193227ff82042dd5636f5dbac187e75acc813926fd06f7fcf`
and source `single.c`
`60e14feef23df3b5fa5b6c8b1797a5f408e8575b82a11025029b1091b3cc168e`.

The attempted two-source link did not complete: chibicc reported
`mkstemp failed: File exists`, then `Hangup`/129. The driver uses multiple
`mkstemp` temporary files per source. The failure is in the compiler/runtime
tool path and needs owner investigation; it is not evidence of ext4 damage.
The test did not include Kilo or Samurai, which are absent from this candidate.

The guest cannot cleanly unmount/shut down with this tuple. In
`linux/arch/myemulator2/kernel/process.c`, `machine_restart()` and
`machine_power_off()` enter an infinite `halt` loop, so the kernel has no
implemented path here to stop or reset QEMU after sync. These tests used
`sync` followed by host QEMU termination and guest journal replay, not a clean
unmount. After QEMU termination, read-only `e2fsck -fn` can report pending
journal/counter discrepancies; replaying the journal on a private copy with
`e2fsck -fy` repaired the metadata, and a subsequent `e2fsck -fn` was clean.
This makes verification of an actual guest clean shutdown the main remaining
persistence risk.

An attempted native `/usr/bin/as` build failed with “Internal error
(Segmentation fault)” and `Hangup` (exit 129), before producing an object.
`mv` returned 139, although its destination later appeared with the exact
expected content after reboot and host recovery; its exit status is therefore
unreliable. `rm` returned 0, and an offline ext4 lookup after shutdown
confirmed the deleted file was absent. A C source replacement returned 0 and
its new hash matched after reboot. These results validate rename/delete and
modification data state, while the anomalous `mv` status remains a guest
runtime/tool issue. No kernel, QEMU, ext4, compiler, shell, or launcher fix
was made in this investigation.

## Backup and recovery procedure

Keep REM stopped and its image detached from QEMU. The helper enforces an
offline check, checks ext4 read-only, copies sparsely, and verifies SHA256:

```sh
tools/rem-ext4-image.sh check /path/to/rem-rootfs.ext4
tools/rem-ext4-image.sh backup /path/to/rem-rootfs.ext4 /safe/path/rem-backup.ext4
tools/rem-ext4-image.sh restore /safe/path/rem-backup.ext4 /tmp/rem-restore.ext4
tools/rem-ext4-image.sh extract /safe/path/rem-backup.ext4 /root/project /tmp/project-recovered
```

The tested 10 GiB development image backup and restored copy both verified as
`b00e07e7ca2f734e231985693295d42846d116f9fc88206d43589406dd608442` before the
restored copy was booted. Host extraction privately copies the image and
replays the journal on that copy; it never modifies an image attached to QEMU.

## Artifacts and status

- `tools/rem-ext4-image.sh`: offline check, backup, restore, and recovery.
- `tools/test-rem-ext4-persistence.py`: repeatable serial workload runner;
  its full workload currently fails on this package because the guest write
  path can fault.
- 53 QEMU guest kernel starts were captured across disposable persistence,
  reboot, restore, failure-isolation, and recovery runs; that count includes
  failed probes and is not 51 successful clean shutdown cycles. Reproduction
  logs, block traces, disposable images, and recovered files are under
  `/tmp/rem-ext4-codex7`.

**Status:** backup/restore, host recovery, journal replay, multi-block writes,
and a single-source guest compile/object/executable persisted and recovered
with matching hashes. Multi-source build, Kilo/Samurai-to-chibicc workflow,
and a clean guest shutdown remain unproven. Require `sync` before stopping REM
and keep a verified offline backup; resolve the multi-source temp-file and
guest shutdown paths before treating the full development workflow as proven.
