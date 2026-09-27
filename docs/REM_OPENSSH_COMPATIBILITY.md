# REM OpenSSH compatibility

**Status (2026-09-27):** controlled OpenSSH 10.5p1 build produced selected
static REM ELF executables, but guest runtime validation **FAILS**: even
`ssh -V` faults. The complete `make` build also **FAILS** in upstream `scp.c`
because `glob_t.gl_matchc` is absent from the selected musl headers. No
OpenSSH, libc, kernel, linker, or flight-image implementation was changed.

The latest network-profile rerun and independent runtime probes are recorded
below. They do not turn the tested tuple into an accepted flight candidate.

## Source and inputs

The source is Portable OpenSSH 10.5p1, the official release dated 2026-08-11.
Downloaded archive:
`/tmp/rem-openssh-current/openssh-10.5p1.tar.gz`, SHA-256
`d44d28a839ea9daf969cc69150fde59910b2b39361dad81a3bd6cbd19218db11`.
This is a release tarball (not a Git checkout), so a Git revision is not
applicable. Its published SHA-256 is
`1E0oqDnqna+WnMaRUP3lmRCys5Nh2tgaO9bL0ZIY2xE=`. [Official release notes](https://www.openssh.org/releasenotes.html)
and [Portable OpenSSH download page](https://www.openssh.org/portable.html)
identify the release and source mirrors.

| Input | Value | SHA-256 |
|---|---|---|
| Target GCC | `.toolchain-install/bin/myemulator2-elf-gcc`, GCC 15.2.0 | `07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe` |
| Linux-musl test wrapper (`CC`) | `/tmp/rem-openssh-current/rem-cc` | `dad5adea7e4c8294c364b137b10b5b6f7672dbbf9168b62b01a11ed1ad283bcb` |
| Target libc | disposable REM musl `libc.a` | `312c6c393d9b879448edd28a822ec4fd56e1f40e364764c5da77a0df884ba21f` |
| Target compiler runtime | `.toolchain-install/lib/gcc/myemulator2-elf/15.2.0/libgcc.a` | `4cc9ae3d89d1600d31f0cc93dca34244c9fd14f027259c6a715e8cdc4f53d1e6` |
| Target linker | `.toolchain-install/myemulator2-elf/bin/ld` | `47686759160071e83b256c5ec35857ee8662dd2ea201aab17909716464772128` |
| OpenSSL headers | staged OpenSSL 3.4.1 | `opensslv.h`: `03cd60b5f285cd2b89fc463f90bc5937bf5415497f2558f0605f98a4a63da5ba` |
| OpenSSL `libcrypto.a` | staged static library | `ca1f35edd0934b6271ffbcba2ea7830baf24db8d84fe78dca6158763c6806cf5` |
| OpenSSL `libssl.a` | staged static library | `4fe1263ac75e3073682e589b3d1b68de3f678f7f40a9e8f9b592d7ebc4f6d372` |
| zlib `libz.a` | staged static library | `353c96e309c41999d6bfbadf89ca61b6fb3a2fe0176838273a476ac3449b36b5` |
| atomic helpers | `libatomic_compat.a` | `e03befb20ce8b678a418dbb1e1ebe10ab20765747e27bdad036cf39749753cdb` |

OpenSSH was cross-configured with build triplet `x86_64-pc-linux-gnu` and
host triplet `i386-pc-linux-musl`. GNU `config.sub` rejects the project name
`myemulator2-linux-musl`, so the configure host uses a recognized Linux
triplet while `CC` emits the REM ELF ABI. The compiler is a small test wrapper
at `/tmp/rem-openssh-current/rem-cc` around the pinned GCC. It adds the REM
musl CRT and linker script for links and supplies the staged headers/libraries.

Configure invocation:

```sh
CC=/tmp/rem-openssh-current/rem-cc \
CFLAGS='-O2 -std=gnu17 -static' \
LDFLAGS='-static -L.../.rem-flight-build-20260924/rootfs/usr/lib' \
LIBS='-lssl -lcrypto -lz -latomic_compat' \
./configure --build=x86_64-pc-linux-gnu --host=i386-linux-musl \
  --prefix=/usr --sysconfdir=/etc/ssh \
  --with-ssl-dir=.../.rem-flight-build-20260924/rootfs/usr \
  --with-zlib=.../.rem-flight-build-20260924/rootfs/usr
```

Configure added these compile flags: `-O2 -std=gnu17 -static -pipe
-Wno-error=format-truncation -Wall -Wextra -Wpointer-arith -Wuninitialized
-Wsign-compare -Wformat-security -Wsizeof-pointer-memaccess -Wno-pointer-sign
-Wno-unused-parameter -Wno-unused-result -Wimplicit-fallthrough
-Wmisleading-indentation -fno-strict-aliasing -D_FORTIFY_SOURCE=2 -ftrapv
-fzero-call-used-regs=used -ftrivial-auto-var-init=zero -fno-builtin-memset
-fstack-protector-strong -fPIE`. Link flags included `-static`, the staged
library search path, `-Wl,-z,noexecstack`, `-fstack-protector-strong`, and
`-pie`; linked libraries were `-lssh -lopenbsd-compat -lssl -lcrypto -lz
-latomic_compat` plus target libc/libgcc from the compiler wrapper.

The omitted `...` prefixes above are the repository root. PAM, Kerberos,
SELinux, and libedit were not explicitly disabled: configure found their
headers/dependencies unavailable in the staged target sysroot and reported
those optional integrations as `no`. The configured sandbox style is `none`
because the target sysroot lacks the Linux seccomp headers. Configure also
assumed several runtime properties because this is a cross build; these
assumptions still require guest tests.

## Build results

The OpenSSL archive references GCC atomic helper symbols. A first link probe
without the staged `libatomic_compat.a` failed with undefined
`__atomic_fetch_add_4` and related symbols. Adding that existing compatibility
archive let configure's OpenSSL link probes succeed; no compatibility code was
added during this work.

`ssh`, `sshd`, `ssh-keygen`, `sshd-auth`, `sshd-session`, and `sftp-server`
were emitted as static ELF32 REM-machine (`0xf2e2`) files. The selected
client/server/keygen outputs have these hashes:

| Binary | SHA-256 |
|---|---|
| `ssh` | `100e38d3ea49958abd304e4988f6a5cc57776f884d9d9674939d45d88d45629c` |
| `sshd` | `48709537cdee943a577d4c53fc989f55448982ed9913de0a90cc97a3a566c59e` |
| `ssh-keygen` | `5f76e88f661a1ad740a74e0d5d98be04442605ace24b92ad0ed93e11ae06791a` |
| `sshd-auth` | `d3922c4895e8f108eee70e03fdafc07f3c6d408bef7361f686c56be00c3a3a0a` |
| `sshd-session` | `67ee5e0cd6777c568e1e241d4b6293a115731254f29975c401ae2b13182ebc56` |
| `sftp-server` | `6edd5db0c647ce4d6e06e6512d1e728c8b4de08ae7b761db49a2d57dde893030` |

These outputs are not yet accepted runtime artifacts. The link log includes
`myemulator2-elf-ld: error in .../libgcc.a(_divdi3.o)(.eh_frame); no
.eh_frame_hdr table will be created` (also `_moddi3.o`, `_udivdi3.o`, and
`_umoddi3.o`) and reports an RWX LOAD segment. The ELF files exist and are
recognized as REM ELF, but guest execution must determine whether they are
usable; the linker diagnostics are recorded, not suppressed.

The full `make` failed in upstream `scp.c` at lines 1574, 1591, and 2021:
`glob_t has no member named 'gl_matchc'`. Configure tested for this extension,
reported it absent, and selected the non-system glob path, but `scp.c` still
references the extension. This is an application/build integration failure
against the target libc API, not evidence of compiler-generated-code failure.
No source workaround or feature removal was applied. Reproducer is the
unmodified OpenSSH 10.5p1 source plus the configure/build commands above;
the latest complete build log is `/tmp/rem-openssh-current/make-full.log`.

## Guest acceptance harness

The test-only harness is
[`test-openssh-rem.py`](../toolchain/scripts/test-openssh-rem.py). It pins
QEMU/kernel/rootfs hashes, copies the rootfs before staging files, records
binary hashes, boots the candidate, and leaves the source image unchanged.
It attempts:

- `ssh -V`;
- host and client Ed25519 key generation and private-key mode check;
- `sshd -t` with a disposable configuration;
- daemon start, localhost public-key authentication and command execution;
- localhost forced-PTY command and daemon signal shutdown.

Run against an exact pinned base tuple:

```sh
python3 toolchain/scripts/test-openssh-rem.py \
  --tuple tools/rem-flight-image/tuples/flight-20260927-chibicc-level5.json \
  --build-dir /tmp/rem-openssh-20260927/openssh-10.5p1
```

Or test a candidate manifest and its output rootfs:

```sh
python3 toolchain/scripts/test-openssh-rem.py \
  --candidate-dir /tmp/rem-flight-candidate-20260927-chibicc-final3 \
  --build-dir /tmp/rem-openssh-20260927/openssh-10.5p1
```

The documented base rootfs path had drifted from its pinned hash when run:
the manifest expected `46604d…`, while the actual file was `04eaef…`; the
harness refused to boot it. The candidate output also changed from its
manifest-pinned `c9f19f…` to `112749…`. The explicit
`--refresh-candidate-rootfs-hash` option permits testing such a changed
candidate only after checking its manifest-listed guest files; the report
records both hashes. This is not acceptance of the changed candidate.

The guest run booted to the shell, but the OpenSSH processes faulted before
account, networking, or terminal behavior could be validated. The candidate
seed rootfs has no `/etc/passwd` or `/etc/group`; the harness stages minimal
root and `sshd` records and `/var/empty` only in its disposable copy for a
future run that reaches those paths. This fixture does not alter the candidate
or flight package. The source rootfs hash after the run remained
`1127493c4abe15fc9bf9993f34c5d8d4b1d23c73fd8c3ff1c4c2282e5d52ae8d`.

### Guest results

Exact tested tuple: QEMU `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`;
kernel `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`;
candidate rootfs actual `1127493c4abe15fc9bf9993f34c5d8d4b1d23c73fd8c3ff1c4c2282e5d52ae8d`
(candidate manifest had pinned `c9f19fab0d27b7a96bec09bf09fc38f37059f7e462d2b9ca4e68bf57fb61c0de`).
The candidate manifest's listed guest files passed verification against the
actual rootfs before launch. The guest booted successfully, and host commands
and guest response markers traversed the serial console in both directions.
This is a test of
the explicitly refreshed, modified candidate rootfs, not a pass for the
manifest-pinned rootfs or a flight candidate.

`ssh -V` exited 139 with `MYEMU_BAD_USER_FAULT` at PC `02000180`, address
`00001070` (`ssh` `main+0xac`). `ssh-keygen` exited 139 at PC `020e1688`,
address `0000023c`; target symbolization points into
`evp_default_properties_enable_fips_int`. `sshd -t` exited 139 at PC
`0203665c`, address `00000574`; symbolization points to `sshkey_private_to_blob2`
(`sshkey.c` has no line mapping). The captured serial log is
`/tmp/rem-openssh-20260927/serial.log`; machine-readable per-command results
and ELF hashes are in
`/tmp/rem-openssh-20260927/guest-report-final-rerun.json`. The rerun after
fixing the harness's authorized-key staging check remained **FAIL**; that
check now correctly reports failure when key generation did not produce its
public-key file.

These faults occur in version-only, key-generation, and configuration paths,
before a successful socket operation. Classification is **compiler-generated
code / ABI / libc-runtime unresolved**; this evidence does not isolate which
layer is responsible. Networking (socket, bind, listen, accept, localhost and
remote connection), key permissions, user lookup, daemon startup, signals,
and PTY allocation are **not validated**, because prerequisite executables
faulted. A `stat` invoked by the key-permission check also faulted, so that
check says nothing about permission semantics. A `MYEMU llist bad add` kernel
diagnostic appeared during fault handling; causal relation is unknown and no
kernel defect is established by this run.

## Failure classification and boundaries

| Observation | Classification | What it demonstrates |
|---|---|---|
| `scp.c` does not compile against target `glob_t` | Application/build integration; libc API mismatch | `scp` is not built; no runtime conclusion about the SSH client/server follows. |
| OpenSSL library link probe needs `libatomic_compat.a` | Runtime/toolchain dependency | This exact static OpenSSL archive requires atomic helper symbols. Guest success is not yet established. |
| `.eh_frame_hdr` and RWX linker diagnostics | Tooling/integration | The generated ELF header is valid, but these diagnostics prevent treating the link as clean acceptance. |
| Candidate rootfs hash differs from manifest | Tooling/integration | Exact rootfs identity must be refreshed and guest file hashes checked before execution. |
| `ssh -V`, `ssh-keygen`, and `sshd -t` fault with `MYEMU_BAD_USER_FAULT` | Compiler/backend vs ABI vs libc/runtime unresolved | Commands fail before networking; captured PCs and log above. Minimal reproducer is the exact hashed binary and command on the exact tuple recorded above. No confirmed root cause. |
| `MYEMU llist bad add` during user-fault handling | Kernel/process diagnostic, causality unknown | Observed in serial log near two faults; insufficient evidence to attribute the user faults or establish a kernel defect. |
| Key permissions, account lookup, daemon startup, sockets, signals, and PTY | Not reached / not validated | Prerequisite OpenSSH programs faulted; do not count the shell prompt as success. |

Remote-host connectivity is not tested by this harness; QEMU external TCP
completion remains separately pending in
[`REM_NETWORKING.md`](REM_NETWORKING.md). No production artifact was changed.

## Current disposable network-profile rerun (2026-09-27)

The old `REM-FLIGHT-TTY/SHA256SUMS` no longer matches the kernel and rootfs on
disk. To continue testing without silently accepting drift, the harness was
given an explicit temporary tuple manifest at
`/tmp/rem-openssh-current/tuple-rem-network-profile.json`. It is an
**unaccepted integration profile**, not a unified candidate:

| Artifact | SHA-256 |
|---|---|
| QEMU `.qemu-build/qemu-system-myemulator32` | `60d869564f26ed2d63f20830a291b67443cf323faa3d3f4b81a94f52534e6882` |
| Kernel `.rem-phone-deploy-20260925/linux-network-build/vmlinux` | `1aa9d65e2ba5910a197ebed06e0189b87723d23c04b9594bcfdf0bc01bad278b` |
| Rootfs `REM-FLIGHT-TTY/rootfs/rem-rootfs.ext4` | `eff360514d9bb49d214cf2b60dd656524fdff493e7354249e5edf8e7d43f12ce` |
| runtime probe `toolchain/tests/openssh-runtime-probe.c` build | `73f1d80bca8e9e29ff35a9e9b3e811993dbc0dd1b84e2da57c541b75a515761f` |
| `rem-cc` wrapper | `dad5adea7e4c8294c364b137b10b5b6f7672dbbf9168b62b01a11ed1ad283bcb` |

The QEMU profile enables the machine's built-in virtio-mmio network device
with `-netdev user,id=net0`; it does not provide a guest IP configuration.
The kernel command line contains `ip=off`. Therefore the probe's
`connect(127.0.0.1)` returned `ENETUNREACH` (errno 101). This is a networking
setup boundary, not evidence that TCP connect works or that the networking
implementation is defective. External TCP, DNS, HTTP, and SSH transport remain
unvalidated and are pending the networking owner.

The probe separately confirmed these guest interfaces on this tuple:

| Probe | Result | Evidence / boundary |
|---|---|---|
| `socket(AF_INET, SOCK_STREAM)` | PASS | Socket returned a descriptor. |
| `bind`, `listen`, `getsockname` | PASS | IPv4 server socket bound an ephemeral port. |
| `accept` | PASS | Nonblocking call returned expected `EAGAIN` with no peer; this proves the API call path, not a completed connection. |
| `connect` | FAIL, `ENETUNREACH` | No guest address/route under `ip=off`; transport untested. |
| `fork` + `execv(/bin/busybox true)` + `waitpid` | PASS | Child exited successfully. |
| signal handler delivery across `fork` | PASS | Child handled `SIGUSR1` and exited. This does not validate OpenSSH's daemon signal lifecycle. |
| private mode from `umask(077)` and `fstat` | PASS | Created file mode was `0600`; BusyBox `stat` itself faults, so this conclusion comes only from the C probe. |
| UNIX98 PTY master/slave via devpts | PASS | `ptmx`, `grantpt`, `unlockpt`, `TIOCGPTN`, slave open, and `tcgetattr` succeeded. This does not validate SSH PTY negotiation. |

`ssh -V`, RSA `ssh-keygen` (host and client keys), `sshd -t`, and `sshd`
startup still **FAIL** with status 139 before their intended behavior can be
tested. Current fault evidence:

| Command | Fault evidence | Classification |
|---|---|---|
| `ssh -V` | `MYEMU_BAD_USER_FAULT`, PC `02000180`, address `00001070`, `ssh main+0xac` | Compiler-generated code / ABI / libc-runtime unresolved; not networking. |
| `ssh-keygen -t rsa -b 2048` | PC `020e1688`, address `0000023c`; symbolization points into `evp_default_properties_enable_fips_int` | Compiler-generated code / ABI / crypto runtime unresolved. |
| `sshd -t` and daemon startup | PC `0203665c`, address `00000574`; symbolization points to `sshkey_private_to_blob2` | Compiler-generated code / ABI / crypto runtime unresolved. |
| BusyBox `stat` used by the separate mode check | PC `02005250`, address `00000434` | Existing utility fault; the C `fstat` mode probe independently passed. |

The harness correctly marks key generation, key-mode check via `stat`, config
validation, authorized-key staging, daemon startup, localhost authentication,
SSH PTY, and daemon shutdown as **FAIL / not reached**. The client-key file was
not produced, so the stricter `&&` staging command fails instead of reporting
a false success. Captures are in
`/tmp/rem-openssh-current/guest-report-final-nonblocking.json` and
`/tmp/rem-openssh-current/serial.log`; the full upstream build failure is in
`/tmp/rem-openssh-current/make-full.log`. The disposable image was used for
all staging; the source rootfs hash remained the recorded value above.

The guest also repeatedly logged unknown syscall 407, consistent with the
kernel source's missing `clock_nanosleep_time64` mapping documented in
[REM_ARCHITECTURE64.md](REM_ARCHITECTURE64.md). Its caller is not identified,
and it has not been shown to cause the OpenSSH faults. An ext4 warning was
also emitted during this runtime profile; persistence/image integrity is
outside this OpenSSH result and should be reviewed with the storage owner.

### Current result by requested capability

| Capability | Status |
|---|---|
| OpenSSH source build (`ssh`, `sshd`, `ssh-keygen`, helpers) | Selected targets build; complete build fails at `scp.c`/musl `glob_t.gl_matchc`. |
| `ssh -V`, RSA key generation, `sshd -t` | **FAIL**, user faults above. |
| Socket create/bind/listen/accept API | **PASS** for those individual calls; no completed connection. |
| TCP connect / localhost SSH / outbound SSH | **NOT VALIDATED**; guest has no IP route and OpenSSH binaries fault first. |
| fork/exec/waitpid, simple signal delivery | **PASS** in standalone probe. |
| private file mode | **PASS** in standalone C probe; BusyBox `stat` check faults. |
| UNIX98 PTY allocation | **PASS** in standalone probe; SSH PTY remains unvalidated. |
| OpenSSH permissions/account lookup/server process lifecycle | **NOT VALIDATED**; prerequisites faulted. |
