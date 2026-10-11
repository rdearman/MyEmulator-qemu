# REM TCP / task-work reproducer handoff

Status update (2026-10-11): `rem32-csum-little-endian.patch` is no longer a
private candidate. Its checksum changes are present in the tracked
`linux/arch/myemulator2/lib/checksum.c`. The old statement below that kernel
owner integration was still pending is historical. The qualification here
applies to its recorded candidate tuple only; it does not establish that the
checksum change caused or fixed the separate `task_work_run` fault.

This is the smallest preserved userspace program currently known to reproduce
an unaligned kernel fault on the networking qualification tuple. It configures
`eth0` with a static address and default route, checks local UDP, invokes
`getaddrinfo("example.com")`, and starts a nonblocking TCP connect to
`1.1.1.1:80`.

## Reproducing the observed fault

The disposable tuple that produced the fault was:

- QEMU: `/tmp/REM-0.1-FLIGHT-20260929/qemu-system-myemulator32`, SHA-256
  `34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`
- Kernel: `.rem-phone-deploy-20260925/linux-network-build/vmlinux`, SHA-256
  `1aa9d65e2ba5910a197ebed06e0189b87723d23c04b9594bcfdf0bc01bad278b`
- Rootfs: `/tmp/rem-network-qualification-20260929/rootfs.ext4`, SHA-256
  `e300da74193bbfd8304ac3d328c01b7e96cef2f4b8070d31951968643a4dab7f`
- Probe source SHA-256: `0352b2910c2c8c77c88c379023020720a8fe3f64500635f4eb2d84426b14b85c`
- Staged probe binary SHA-256: `c38f883bba626466a8418a6acaaafbab24e28b487a09afa4acdec001af9eab6d`

The guest was launched with `-netdev user,id=net0`, `-icount shift=0,sleep=off`,
and `-no-reboot`. Its `/sbin/init` runs `/root/netprobe`. In the captured
failure, `connect()` returned `-EINPROGRESS`; before userspace resumed, the CPU
faulted at `task_work_run+0xc4` (`lw r5,0(r5)`) with exception address
`0x10046003` and supervisor mode set. `task_work_run()` had loaded that invalid
head from `current->task_works` (offset 856). The kernel build has no debug
line information (`CONFIG_DEBUG_INFO_NONE=y`).

## Reduced controls

On the same kernel and QEMU binary, separate disposable probes showed:

- `/etc/passwd` open/close: passes.
- Static interface/route setup then nonblocking TCP connect: returns
  `EINPROGRESS` and userspace resumes.
- DNS timeout then TCP connect: returns `EINPROGRESS` and userspace resumes.
- Failed loopback UDP test then TCP connect: returns `EINPROGRESS` and userspace
  resumes.
- Combining the loopback UDP test, failed DNS lookup, and TCP connect caused a
  kernel exception again, but at a kernel-stack PC with cause 0, not the same
  `task_work_run` unaligned fault.

The exact task-work fault has not yet been reduced below the preserved probe,
and the reduced combined exception is not proven to have the same cause. No
specific corrupting writer, TCP skb alignment error, or user-copy defect has
been established.

## Checksum carry reproduction and passing candidate (2026-09-30)

The original failing tuple above had invalid IPv4/UDP/TCP checksums and timed out
DNS/TCP. A private source copy was used for the fix; the shared kernel source,
flight image, and release package were not changed.

### Minimal observed defect

The existing `netprobe.c` DNS request is 28 bytes when querying `example.co`.
The kernel copied those bytes intact. During the software checksum path,
`csum_and_copy_from_user()` seeded `csum_partial()` with `0xffffffff`. The
architecture implementation accumulated into `u32`, wrapped while adding the
payload, and returned `0x2cb`; an independent sum over the captured bytes is
`0x2cc`. This exact one-unit deficit caused the emitted UDP checksum to be
invalid. A 64-bit accumulator plus repeated end-around folding fixed the
checksum. The candidate patch is
[`rem32-csum-little-endian.patch`](rem32-csum-little-endian.patch).

### Candidate qualification

The candidate kernel SHA-256 was
`e2b7fe6672ee1e37b849815e6fc263844036c44c0255d85a10f67251a15041d1`. With the
same QEMU binary SHA-256
`34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`:

- Static `eth0` configuration, `10.0.2.15/24`, and default route passed.
- DNS through QEMU's `10.0.2.3` resolver passed for `example.com`; capture
  contained a query and reply with valid UDP checksums.
- TCP connect to `1.1.1.1:80` passed; captured TCP checksums were valid.
- A separate guestfwd HTTP fixture completed request/response and returned
  `REM_NETWORK_FIXTURE_OK` to the REM process.
- The combined probe did not reproduce the earlier unaligned
  `task_work_run` panic. This is a correlation; exclusive causation is not
  established.
- `LOCALHOST_UDP=FAIL` remains in the probe. Loopback interface setup was not
  part of this network acceptance subset.

DNS/TCP rootfs SHA-256:
`321c621984f34527624928e657a0b578c848a6b663c6140083cefaa162f1aad3`.
HTTP fixture rootfs SHA-256:
`56af69ae7097ae632d1ec7ff6e32c92b5ad30681c1548516b980dc0d868a8145`.
Full commands, PIDs, output, and cleanup records are preserved in
`/tmp/rem-network-qualification-20260929/qemu-final-dns10.record` and
`qemu-final-guestfwd.record`. Both runners terminated and reaped QEMU; a final
process check found no QEMU instances.

The remaining integration action belongs to kernel owner #1: apply the patch to
the shared Linux source, rebuild the validated candidate tuple, and rerun the
DNS/TCP/HTTP acceptance tests. The source patch passed `patch --dry-run` against
the original snapshot.
