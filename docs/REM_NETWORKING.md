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
