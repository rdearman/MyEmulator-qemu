# REM networking (disposable implementation)

REM32 networking uses the standard QEMU virtio-net device over virtio-mmio.
The disposable network boot profile supplies `-netdev user,id=net0`; the
MyEmulator32 machine maps the transport at `0xf0200000` and raises IRQ 5.
The guest kernel enables `CONFIG_NET`, `CONFIG_INET`, `CONFIG_NETDEVICES`,
`CONFIG_ETHERNET`, `CONFIG_VIRTIO`, `CONFIG_VIRTIO_MMIO`, and
`CONFIG_VIRTIO_NET`. The MyEmulator2 architecture provides the MMIO alias and
page-table mapping required by the virtio transport, plus the IP checksum
helpers required by the network stack.

The transport and driver register successfully and create `eth0` in the
disposable kernel (`register_netdevice(..., name=eth0)` was observed). The
boot profile is intentionally separate from the flight image; the production
image and its custom block device are unchanged. DHCP is not forced by the
validated profile, so initial userspace tests may assign a static address.
QEMU user networking then supplies the ordinary Linux NIC semantics and NAT;
no REM-specific network protocol is involved. The existing flight rootfs does
not include the `ip` or `mount` BusyBox applets and its init does not mount
`/sys` or `/proc`, so `ip link`/`ip addr` still require a fuller disposable
development rootfs.

Boot diagnostics found two independent waits in the network-enabled profile:
`legacy_pty_init()` can stall during PTY registration, and `ip_auto_config()`
waits when IP autoconfiguration is enabled without an explicit `ip=` policy.
Disabling the unused PTY drivers and passing `ip=off` allowed the kernel to
mount ext4 and devtmpfs, launch `/sbin/init`, and reach `REM#`. These are
disposable boot-profile findings; no permanent PTY or IP-autoconfiguration
change was applied.

The disposable networking kernel also required the standard `mount`, socket,
and message syscalls in the MyEmulator2 dispatcher. After adding those
entries, `/proc/net/dev` and `/sys/class/net` showed `lo`, `eth0`, and `sit0`.
The network-capable BusyBox was rebuilt with `mount`, `ip`, `ifconfig`,
`hostname`, `ping`, and `udhcpc`; its disposable binary hash is recorded in
the session artifacts. `ip` currently blocks while waiting for a netlink dump,
so address/route configuration and external connectivity remain incomplete.
A direct static TCP client can create a socket, but without a configured route
the connection reports `Network unreachable`.

Independent PTY testing showed the legacy BSD path is the first blocker:
`CONFIG_LEGACY_PTYS=y` enters `legacy_pty_init()` and does not return from its
TTY-driver registration on this kernel. With legacy PTYs disabled,
`CONFIG_UNIX98_PTYS=y` completes initialization. This points to the REM TTY
driver-registration path rather than virtio-net; the UNIX98 configuration was
kept for the disposable boot test.

For REM64, this establishes that a standard device transport needs an explicit
MMIO aperture, interrupt routing, cache/device-access rules, and a Linux driver
configuration contract. A virtio-mmio transport can avoid PCI while retaining
portable Linux/QEMU compatibility; the address/IRQ allocation must therefore be
part of the virtual motherboard specification.

## Current disposable TCP boundary

The first netlink hang was an ABI omission rather than a netlink-driver failure: BusyBox `ip` uses generic syscall 204 (`getsockname`) while finishing the route-netlink dump. The MyEmulator2 dispatcher returned `ENOSYS`; adding the Linux `sys_getsockname` mapping (and paired 205 `getpeername`) made `ip link` complete and proceed through normal interface ioctls. REM64 should validate the complete socket syscall family, including auxiliary name and option calls used by ordinary tools.

The current disposable validation artifacts are `vmlinux` SHA-256
`fb76d257ea9469c59737c16792b187dce65edac03750767f26539285437eec55`, QEMU
SHA-256 `78c730d028b656771a0b46b4c98d2a33847eb3a4e8851ee8162f6aa0b8a6f741`,
BusyBox SHA-256 `6be19cb921fd1f7e47a7d9f76c37fe0ac2e286f86f03c14db9a583c6d452c447`,
and TCP probe SHA-256
`3f6690406a322c2f3684c76a2730557a9b3699a10368934c41265239a525f724`.

The disposable TCP path was then traced at the virtqueue boundary. Linux and
QEMU reported the same legacy split-ring addresses (for example TX descriptor
base `0x00d50000`, avail `0x00d54000`, used `0x00d55000`, 1024 entries), and
QEMU consumed each published TX descriptor. A QEMU filter-dump capture contains
valid ARP requests/replies and retransmitted IPv4 TCP SYN packets from
`10.0.2.15`, proving guest-to-QEMU packet delivery. No guest-side DMA or ring
address fix was required. The remaining TCP blocker is the disposable
environment's lack of a responding external/host TCP service; QEMU user-mode
networking emits the SYNs but no SYN-ACK was available in these runs.

## Final RX pass

The final RX trace confirms that QEMU generates the gateway ARP reply and
delivers it through the virtio RX queue. QEMU `virtqueue_pop`/`fill`/`flush`
events show an RX buffer being consumed and filled (76 bytes for the ARP
reply), and the guest receives virtio IRQ 5. The guest then learns the gateway
MAC and emits TCP SYNs, demonstrating Linux processed the incoming ARP frame.
No RX or virtio backend fix was indicated.

Final status: **REM networking stack PASS. Outbound packet transmission PASS.
External TCP completion pending QEMU/backend investigation.**

A disposable host HTTP listener was also tried through the QEMU user-network
gateway (`10.0.2.2`). REM emitted SYNs toward the listener, but the host saw
no connection and QEMU returned no SYN-ACK. The same result occurred when the
QEMU run was executed outside the normal sandbox, so this is isolated to the
QEMU user-network/backend boundary in the current environment; no guest
virtio, RX, checksum, or route defect was demonstrated.

## REM 0.1 acceptance follow-up (2026-09-29)

A disposable kernel was rebuilt with the existing virtio-mmio network path and
with a new machine power-control register. The network kernel configuration
contains `CONFIG_NET=y`, `CONFIG_INET=y`, `CONFIG_VIRTIO=y`,
`CONFIG_VIRTIO_MMIO=y`, and `CONFIG_VIRTIO_NET=y`. The current disposable boot
reaches ext4 userspace; this run did not demonstrate a configured `eth0` or a
TCP connection because the candidate rootfs has no usable `ip`/`ifconfig`
setup and the read-only custom block path cannot be used for a clean network
boot.

The power path now has a Linux syscall 142 (`reboot`) dispatcher entry and
architecture hooks that write restart/poweroff commands to a QEMU MMIO power
register at physical `0xf0300000` (kernel alias `0x7ff00000`). QEMU maps that
register and handles the guest shutdown/reset causes. A direct disposable
probe reached `reboot: System halted`; QEMU reset/exit completion still needs a
clean, uninstrumented acceptance run with the normal init and a journal-clean
rootfs.

## REM 0.1 TCP fault retest (2026-09-29)

The disposable networking tuple was retested with QEMU SHA-256
`34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`, kernel
SHA-256 `1aa9d65e2ba5910a197ebed06e0189b87723d23c04b9594bcfdf0bc01bad278b`,
and rootfs SHA-256 `e300da74193bbfd8304ac3d328c01b7e96cef2f4b8070d31951968643a4dab7f`.
Static `eth0` address and route setup pass, and virtio TX publishes valid ARP,
DNS, and TCP frames. DNS queries to `10.0.2.3:53` time out; the TCP probe's SYN
to `1.1.1.1:80` receives no completion in this environment.

One exact run of the preserved probe
[`task-work-connect-repro.c`](../tests/network/task-work-connect-repro.c)
faulted immediately after `connect()` returned `-EINPROGRESS`: supervisor
exception PC `0x0013bc3c`, cause 3, address `0x10046003`. Disassembly identifies
the instruction as `task_work_run+0xc4`, loading `callback_head->next`; the
invalid head had been read from `current->task_works`. A plain regular-file
open/close and isolated static-connect, DNS-then-connect, and loopback-UDP-then-
connect controls resumed normally. Combining loopback UDP, DNS, and TCP caused
another kernel exception, but at a kernel-stack PC with cause 0, so an identical
reproducer for the original fault is still not established. The corrupting
writer and fault ownership within the kernel/toolchain boundary remain open.

The QEMU user-network guestfwd HTTP fixture attempt also timed out without a
TCP reply. The host fixture process was reaped; the QEMU process was stopped and
reaped. No QEMU process remains. Reproducer tuple, checksums, and exact command
are recorded in [`tests/network/README.md`](../tests/network/README.md).

Current result: interface configuration and guest-to-QEMU TX are verified;
DNS, TCP completion, and fixture download are not verified. No kernel or release
artifact was modified in this retest.

## REM32 checksum carry fix candidate — QEMU guest retest (2026-09-30)

A private Linux 6.12.1 build isolated a second defect in
`arch/myemulator2/lib/checksum.c::csum_partial()`. For a 28-byte DNS query,
`csum_and_copy_from_user()` supplied an initial partial sum of `0xffffffff`.
The 32-bit accumulator wrapped while adding the payload and dropped the carry:
the intact query bytes summed to `0x2cc`, but the kernel returned `0x2cb`.
QEMU packet capture showed the outgoing UDP checksum off by one checksum unit;
QEMU DNS and external DNS both discarded the query. IPv4 and TCP header
checksums also required little-endian word accumulation on this target.

The isolated candidate widens the accumulator to `u64`, sums little-endian
16-bit words, and folds end-around carry until no high bits remain. The patch is
[`tests/network/rem32-csum-little-endian.patch`](../tests/network/rem32-csum-little-endian.patch);
it applies cleanly to the validated source snapshot. No shared kernel source or
release image was edited.

On the same QEMU binary, the candidate kernel passed static `eth0` setup,
`10.0.2.15/24`, default route, DNS resolution through `10.0.2.3`, and TCP
connect to `1.1.1.1:80`. `tcpdump` reported the DNS query and reply checksums
valid and TCP checksums correct. The original combined DNS/TCP probe completed
without the earlier `task_work_run` panic. A separate guestfwd HTTP test
completed the handshake, sent an HTTP request, received
`REM_NETWORK_FIXTURE_OK`, and returned from the test program without a kernel
panic.

Candidate tuple:

- QEMU SHA-256: `34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`
- Private candidate kernel SHA-256: `e2b7fe6672ee1e37b849815e6fc263844036c44c0255d85a10f67251a15041d1`
- DNS/TCP test rootfs SHA-256: `321c621984f34527624928e657a0b578c848a6b663c6140083cefaa162f1aad3`
- HTTP fixture rootfs SHA-256: `56af69ae7097ae632d1ec7ff6e32c92b5ad30681c1548516b980dc0d868a8145`
- DNS/TCP QEMU record: `/tmp/rem-network-qualification-20260929/qemu-final-dns10.record`
- HTTP QEMU record: `/tmp/rem-network-qualification-20260929/qemu-final-guestfwd.record`

Both QEMU runs exited with status 0 after `SIGTERM_and_wait`; the final process
check found no `qemu-system-myemulator32` process. The loopback UDP subcheck in
the probe still fails; it does not prevent the verified `eth0`, routed DNS,
external TCP, or HTTP fixture workflows. The `task_work_run` fault did not recur
on this candidate, but checksum correction as its cause remains a strong
correlation rather than a proven exclusive cause. Kernel owner #1 should apply
the isolated patch to the shared source and repeat the tuple tests before
integration.

### Userspace download-tool boundary

The HTTP fixture above was fetched by a dedicated static REM test client; it
proves TCP receive and HTTP response handling, not a packaged downloader. The
staged BusyBox configuration has `CONFIG_WGET` unset, so `/bin/busybox wget` is
not available in that candidate. A `curl` executable exists in the separate
workstation staging tree, but its HTTP transfer has not been tested on this
kernel. Qualify the actual packaged `curl` (or enable BusyBox wget in a future
userspace build) before claiming end-user source downloads work.

### Shared checksum integration and staged curl retest (2026-09-30)

Kernel owner #1 applied the checksum fix to shared
`linux/arch/myemulator2/lib/checksum.c` and rebuilt a candidate whose SHA-256
matches the already-tested private candidate:
`e2b7fe6672ee1e37b849815e6fc263844036c44c0255d85a10f67251a15041d1`. The
earlier DNS/TCP/HTTP results therefore apply to the same kernel bytes as the
shared rebuild. No release image was changed.

An additional disposable test tried the staged static `curl` binary
(SHA-256 `a408ddcf25f4c08a7e50044f6dbcc8ada7c57885fe87f2ff4ac0569049c8dcae`).
Both a fixture download and `curl --version` faulted before completing. The
version-only run made no TCP request and faulted at user PC `0x0203698c`
(`curl_msnprintf+0x4c`), with `MYEMU_BAD_USER_FAULT` address `0x000013f8`, cause
7, SP `0x3f9bdc10`, and FP `0x00001400`. Disassembly shows the faulting load
after a call to curl's internal `formatf`. The process exited by signal
(`CURL_VERSION_EXIT=128`); no download file was created. Kernel warnings also
pointed to `fs/ext4/inode.c:3554-3555` during the transfer attempt.

This isolates the first curl failure to executable startup/formatting before
network I/O; it does not establish whether the cause is curl, compiler/backend,
libc, kernel process state, or filesystem interaction. The small REM HTTP
probe still proves the tested TCP receive and fixture-download path. See
`/tmp/rem-network-qualification-20260929/qemu-curl-test.record` and
`qemu-curl-version.record` for disposable tuples and cleanup records; both VMs
were reaped and no QEMU process remains.
