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

## P0 source-transfer runtime isolation (2026-09-27)

The required path is encrypted SSH/SCP source transfer. The failure was
isolated before attempting another SSH connection. Tests used a fresh copy of
the tuple rootfs and left the source image hash unchanged. The tuple remains
unaccepted, and its kernel has no configured guest IP route.

### Minimal dependency results

| Test | Result | What this establishes |
|---|---|---|
| `malloc`, `memset`, `strlen`, `memcmp`, `free` | PASS | This basic allocation/string sequence survives in a GCC-built static REM executable. It does not clear all libc/runtime paths. |
| zlib `deflate` + `inflate` round trip | PASS | Compression and decompression work in the corrected test with exact compressed length and sufficient output buffer. |
| `OpenSSL_version_num()` | PASS | A basic libcrypto query returns. |
| `OPENSSL_init_crypto(ADD_ALL_CIPHERS | ADD_ALL_DIGESTS, NULL)` | PASS | The same initialization call used by OpenSSH's `ssh_libcrypto_init()` returns 1. |
| `RAND_status()` immediately after that initialization | FAIL, status 139 | No `RAND_status` result marker is printed. GCC probe faults in OpenSSL `provider_conf_init` at PC `02068320`, invalid address `0000023c`. |
| One-shot `SHA256("abc", 3, ...)` after OpenSSL initialization | FAIL, status 139 | The earlier isolated GCC probe faulted in OpenSSL `parse_value` (`property_parse.c`), PC `0203ac68`, invalid address `0000042c`. OpenSSL's high-level digest selection/provider path is therefore not usable. |

The standalone test source is
[`openssh-runtime-isolation.c`](../toolchain/tests/openssh-runtime-isolation.c).
SHA-256 `cc0e59bf7c2e9a52a52d2cc4df39894145d86b674b9ad22177be1e3719c90894`.
Its linked GCC binary was
`/tmp/rem-openssh-current/runtime-isolation-gcc` (SHA-256
`e730eeaf2e41005d0e66ebcce6bef969bbf8e8acb077fd8ada6a2488893718b9`).
The paired compiler comparison uses the same minimal call sequence from
[`openssh-rand-isolation.c`](../toolchain/tests/openssh-rand-isolation.c),
SHA-256 `5a1813334a8e86605934ecfe22874050243c114f78337662bbbd7919f9c06bf0`:

| Input | SHA-256 |
|---|---|
| REM GCC 15.2.0 | `07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe` |
| Host chibicc REM backend driver | `1d42607078ad852e3f852cb7639b6fc0f0ff033fcba9a6bd2036a3e14fca1631` |
| `rand-probe-gcc` | `7b9c0457826270c256e0ffbabd38fdd80235df07c8d987bb547ed7373fbcd5dc` |
| `rand-probe-chibicc` | `5a013fcd2e8b3ab9fff86485a4a339947caef1485282f722164e79b3fe4f3c52` |
| Shared target `libcrypto.a` | `ca1f35edd0934b6271ffbcba2ea7830baf24db8d84fe78dca6158763c6806cf5` |
| Shared target `libz.a` | `353c96e309c41999d6bfbadf89ca61b6fb3a2fe0176838273a476ac3449b36b5` |

The GCC isolation build used the pinned `rem-cc` wrapper with
`-std=gnu17 -O2 -static`, target OpenSSL headers, and
`-lcrypto -latomic_compat`. The same source was built with the host
`toolchain/chibicc-rem/chibicc` REM backend at `-O2`, using the pinned REM
assembler, linker, linker script, CRT, libc, libgcc, and the same two target
archives. Both final programs are static REM ELF32; their hashes are above.

The paired guest transcript is
`/tmp/rem-openssh-current/rand-pair-serial.log` (SHA-256
`036bf20e03efa2cec574a9c4de0602d143ca93649d1147a5311f6cc3b810721c`).
The source rootfs remained unchanged at SHA-256
`eff360514d9bb49d214cf2b60dd656524fdff493e7354249e5edf8e7d43f12ce`.
As a configuration isolation check, the GCC probe was rerun with both
`OPENSSL_CONF=/dev/null` and `OPENSSL_CONF=/nonexistent`; both still faulted
with status 139 in `parse_value` at PC `02080148`, address `0x42c`. This makes
the staged `/etc/ssl/openssl.cnf` an unlikely trigger; it does not identify the
lower-level cause. The transcript is
`/tmp/rem-openssh-current/rand-config-serial.log`, SHA-256
`dbfada7af2ebb9348068f0f6ef230415173a367c17ccf86dc2018abcc21cfa0f`.

Both compiler-built binaries print `RAND_PROBE_INIT_PASS` and
`RAND_PROBE_BEFORE_STATUS`, then terminate with SIGSEGV/status 139 before
printing a status result. In the GCC executable, PC `02068320` symbolizes to
OpenSSL `provider_conf_init` (`provider_conf.c:?`); the bad address is
`0x23c`. The matched chibicc run shows the same call-boundary behavior but no
separately attributable fault PC in its captured transcript. The earlier
chibicc high-level SHA-256 call also faulted inside OpenSSL (`dynamic_finish`,
PC `0202c5b8`, address `0x23c`). Because both binaries call the same GCC-built
libcrypto archive and fail after returning from initialization, this rules
out a failure unique to the C caller compiler. It does **not** distinguish a
libcrypto generated-code defect from a REM ABI/libc/runtime incompatibility.

### OpenSSH startup boundary and transfer status

Portable OpenSSH `ssh.c:main()` calls `seed_rng()` before parsing command-line
options. `entropy.c:seed_rng()` calls `ssh_libcrypto_init()` and
then `RAND_status()`. Therefore `ssh -V` reaches the same failing RNG-readiness
path before it can print its version; the early fault is after OpenSSL init,
inside the RNG/provider path, not in TCP setup. The exact call sequence is in
OpenSSH 10.5p1 `ssh.c` and `entropy.c`.

Classification: **OpenSSL/libcrypto runtime path, with compiler/backend,
ABI, and libc/runtime as unresolved underlying causes**. The common
precompiled library faults with callers built by both GCC and chibicc, but
OpenSSL's own archive was built with the REM toolchain and is not independently
rebuilt here. zlib and basic heap/string operations pass. No OpenSSH source,
library, kernel, libc, or production image was changed.

The minimum source-transfer workflow remains **BLOCKED**: `ssh` exits 139
before option parsing; `scp` cannot transfer; server authentication and
encrypted transport are not reached. Separately, TCP `connect()` remains
unvalidated because this disposable profile has `ip=off` and no route. Do not
work around this by disabling crypto or using plaintext transfer for flight
source backup.

## OpenSSL progression rerun and entropy boundary (2026-09-28)

A fresh controlled OpenSSL build and a progression harness now refine the
earlier `provider_conf_init` result. On the REM guest, GCC builds pass
`OPENSSL_init_crypto()`, `OPENSSL_init_ssl()`, and explicit default-provider
loading in case 5. A later staged, hash-verified run passed the same init and
provider-load case with the chibicc caller. Thus the old `provider_conf_init`
crash is **not reproduced** by the verified pair. Its original cause is
unknown; artifact staging and tuple identity were not adequately checked in
the earlier run.

The host control was rebuilt against system OpenSSL 3.0.13 (host binary SHA-256
`6fb312c68d28c294692d7c8038db6b555a0bcb0fd41e0db0cd14c3a016ad3c3b`); cases
1–8 all passed. Case 8 adds a fixed, **test-only synthetic** sequence with
`RAND_add`; it is not cryptographically secure and is used only to separate
OpenSSL's readiness result from OS entropy availability. On REM, both GCC and
chibicc case 4 (`init_crypto`, `RAND_status`) return false. Case 8 also returns
false after `RAND_add`, so this does not bypass the guest's unready default RNG.
The guest entropy probe independently recorded nonblocking `getrandom()` as
`EAGAIN` and readable `/dev/urandom` bytes while the kernel warned the source
was not initialized. This makes absent/unfinished OS entropy readiness a
direct blocker to secure SSH operations.

The matching kernel build config has `CONFIG_VIRTIO_MMIO=y` but
`CONFIG_HW_RANDOM` unset (config SHA-256
`19f56538dc4d8e07bb19043f44515af65867d60679ec3956bb6d076e4108f9ec`). The
current MyEmulator32 QEMU machine source constructs virtio-net at
`0xf0200000` when a network backend is present and does not construct a
virtio RNG device. REM `ioremap()` has fixed aliases for UART, block, and that
virtio-net window only. This explains the absence of a guest hwrng endpoint in
the tested setup; it does not establish that adding a virtio RNG device alone
will initialize this kernel's random pool. A prior disposable virtio-RNG
placement attempt did not reach a successful boot and remains unvalidated.

To hold OpenSSL version constant, the same source archive (SHA-256
`002a2d6b30b58bf4bea46c43bdd96365aaf8daa6c428782aa4feee06da197df3`, OpenSSL
3.4.1) was separately configured on the host with `linux-x86_64 no-tests
no-shared` and built by host GCC. The progression binary
`93b6ddad2b8a00e809940cce643e37eb90b17327fc3a793b20321dd113bf3c16` passed
cases 1–8. Its host-built `libcrypto.a` hash is
`ccacb9486e0322d9ce09532bd387b48d480607469817f3e04e64f8d221572bdc` and
`libssl.a` is `153fbd2f3a6b169cf1456369841765ccdcc070f0c2fbd56cdf0199195a259e92`.
This shows these progression calls work for this OpenSSL release in a native
host build; it does not by itself distinguish a REM compiler output issue from
REM libc, ABI, syscall, or entropy-source behavior.

The guest results were repeated with both case-8 ELF files staged into a fresh
copy of the rootfs, then dumped back out of ext4 and SHA-256 checked before
boot. This corrected a weakness in the earlier temporary runner: `debugfs`
could report a missing file while exiting with status zero. The verified GCC
and chibicc executables (hashes below) both pass case 5 (init plus explicit
default-provider load), case 3 (`RAND_poll`), and the `RAND_poll` portion of
case 6. Case 6 then reports `RAND_status()` false. Case 4 reports the same
false readiness result, and case 8 still reports false after fixed test-only
`RAND_add` data. The traced case-3 and case-6 reruns emitted no
`MYEMU_USER_ALIGN` exception. Thus the earlier reported `RAND_poll` frame
fault is **not reproduced** by this staged pair; do not attribute its old PC,
address, or disassembly to these verified binaries.

The earlier diagnostic tuple JSON recorded QEMU hash
`60d869564f26ed2d63f20830a291b67443cf323faa3d3f4b81a94f52534e6882`, but the
QEMU actually executed was `34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`.
The run is not a release candidate. The tuple file names kernel hash
`1aa9d65e2ba5910a197ebed06e0189b87723d23c04b9594bcfdf0bc01bad278b` and
source rootfs hash `eff360514d9bb49d214cf2b60dd656524fdff493e7354249e5edf8e7d43f12ce`.
The isolated verification image used for cases 4 and 8 was
`29290c59a9deecf721689b175298f998abe4167ea7a94dfed27b4b68aec7053b`.
Transcript hashes: verified cases 4/8
`161f10d7f9f65f8e4a5650346b79c29505398751b327674ae5f06a1352779e3a`, traced
case 3 `7aa208f8bda8ff023827c31436389bc9f451c16617fa5daf0993f0304f29c11d`,
and traced case 6
`f25c819c9be6b5d0cfa3bc88b9c13200af0691987ef002c2ba9e857ea2a02d47`.
These runs prove guest behavior under that disposable tuple only.

The progression source is
[`openssl-runtime-progression.c`](../toolchain/tests/openssl-runtime-progression.c),
current source SHA-256 `f43cf19282aa27c9ed2b6c499fd13e563a028a2140177311e9abc32a89e3692b`.
Case-8 REM binaries are GCC
`c73c7e6103c7c874281d7649dc585b748bfb2f101ad9e39be5629e9031993541` and
chibicc `dff3be16974935a84cdae0e305833dd18c291ea1b7040ee6149a974709dc3485`.
The probe run's QEMU actually used SHA-256
`34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`, while
its tuple JSON records a different expected QEMU hash
`60d869564f26ed2d63f20830a291b67443cf323faa3d3f4b81a94f52534e6882`; treat
that run as disposable diagnostic evidence, not a validated tuple. Its kernel
was `1aa9d65e2ba5910a197ebed06e0189b87723d23c04b9594bcfdf0bc01bad278b`, and
the staged source rootfs was `eff360514d9bb49d214cf2b60dd656524fdff493e7354249e5edf8e7d43f12ce`.

OpenSSH remains unaccepted: the prior `ssh -V`, key-generation, `sshd -t`, and
SCP runs were not repeated against this hash-verified progression image. Their
results remain historical diagnostics only; do not assign their faults to the
current verified OpenSSL artifacts. Secure entropy readiness is currently
the first confirmed blocker. Network transfer is untested because the guest
profile has no route. No production component was changed.

## Corrected entropy-header build and current OpenSSH rerun (2026-09-28)

The earlier `RAND_status()` false result had a build-configuration cause. The
OpenSSL target wrapper put the flight rootfs headers before OpenSSL's generated
configuration headers. That selected a stale `opensslconf.h` which enabled
`getrandom()` only under `__linux`; the REM GCC target does not define that
macro, and the stale configuration omitted the `/dev/urandom` fallback. The
corrected wrapper puts the OpenSSL-generated headers first. The preprocessed
`rand_unix.c` then selects the configured OS seed path, including the device
fallback. With the disposable QEMU RNG profile, the exact progression binary
`887ea63694d733dbc51aa95e5fe7a160391b6905703f2e947cb2f85ca2e069d1` passed
case 4 (`OPENSSL_init_crypto(); RAND_status()`) with exit 0. The corrected
static archive hashes are `libcrypto.a`
`45ffd0c7eb1a2451a9e8fa0422bcdd0d33773c6b3bd4751192977b59a8c69000` and
`libssl.a` `d991f027077460274397f00d1d4717c28b89d5dadb33051cf469badd1509a328`.
This is a test-build configuration correction, not a production artifact.

OpenSSH 10.5p1 was rebuilt in a private source extraction against that corrected
OpenSSL stage. The source archive SHA-256 remains
`d44d28a839ea9daf969cc69150fde59910b2b39361dad81a3bd6cbd19218db11`; the
corrected target binary hashes are:

| Binary | SHA-256 |
|---|---|
| `ssh` | `684db7866f88b41fd2cd2cc9fe2acb438e78cea353db42797c27a69ffd5ade05` |
| `ssh-keygen` | `b65f795bc922463ddbe7f61320971b00856fe7d5ed5026c31142e0b51f3ee7fe` |
| `sshd` | `e51ced7895a3aed3ef11d06ce58cf230de47faf3ffeff23355b0aec04004b979` |

On a hash-staged disposable guest (QEMU
`e27356c1d393987ea259c2f566053062cad24076b66f9f34e166cad4deb366a3`, kernel
`ed1fb9a33fa3abe3636aa816ae47a32ceb5ea86df0ce673d9535bdf32995ad6e`, source
rootfs `eff360514d9bb49d214cf2b60dd656524fdff493e7354249e5edf8e7d43f12ce`):

* `ssh -V` **PASS**, prints `OpenSSH_10.5p1, OpenSSL 3.4.1 11 Feb 2025`, exit 0.
* RSA and Ed25519 `ssh-keygen` **FAIL**, SIGSEGV. The verified guest trace is
  `CRYPTO_THREAD_write_lock` at PC `0x02125c1c`, reading address
  `0x2d534544`; its caller is `ossl_method_lock_store` at return PC
  `0x0214e084`. The object field expected to be a lock pointer therefore
  contains the bytes `DES-`. Setting `OPENSSL_CONF=/dev/null` did not change the
  fault. Root cause within OpenSSL-generated code, its internal method-store
  state, or the target runtime remains unisolated.
* `sshd -t` **FAIL**, SIGSEGV at PC `0x023d3974` (`getaddrinfo`), bad address
  `0xfffffef4`, frame pointer `0x570`.
* SSH connection setup **FAIL**, SIGSEGV in the `getaddrinfo` path before a
  connection is established. A usable server was therefore not started.
* `scp` **FAIL**: reports `socketpair: Function not implemented` (ENOSYS), then
  its child `ssh` faults. No source transfer occurred. The SCP binary used in
  this probe was the earlier build
  `e89d610c8710130ec23cf199e0bbc0c2288094f5efd65cdfa5eb665b227b4900`; a
  corrected OpenSSH build could not produce `scp.o` because target `glob_t`
  lacks `gl_matchc` at `scp.c:1574,1591,2021`.

The corrected build attempted all targets. `ssh`, `ssh-keygen`, and `sshd`
linked; `scp` compilation remains blocked by the reported `glob_t` field.
Therefore the corrected RAND readiness is only one passed dependency boundary;
OpenSSH key generation, server setup, and encrypted file transfer remain
unaccepted. The guest image was disposable and the source rootfs hash remained
unchanged. The test profile exercised no external network transfer.

### Follow-up isolation with the corrected libc archive (2026-09-28)

The earlier `getaddrinfo` fault does not reproduce with the newer musl archive
used by the corrected private OpenSSH link wrapper. A minimal numeric-address
`getaddrinfo("127.0.0.1", ...)` probe faulted when linked to the old rootfs
`libc.a` (`30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924`)
and passed at `-O0` and `-O2` with the newer archive
(`312c6c393d9b879448edd28a822ec4fd56e1f40e364764c5da77a0df884ba21f`). The
precise implementation defect in the old archive is not isolated. Thus the
older `sshd -t`/SSH `getaddrinfo` fault above is tuple-specific evidence, not a
current attribution to OpenSSL or GCC.

Using newly rebuilt binaries linked to that newer archive (`ssh` SHA-256
`46adda6189cefaed182f83ccbee77718207483426c6072ee5f71fc936848cfff`,
`ssh-keygen` `6097a06bc60bcbeda280bfbb212f917488e4fc7989437dad4672fd45f312d2ab`,
`sshd` `0fac18671d7d0e71c1c4d676d66b68ed43d7de11e93dae702590ad0306a549e5`),
`ssh -V` passes. `ssh-keygen -t ed25519` did not complete within 180 seconds;
classify this as **TIMEOUT**, not a reproduced SIGSEGV. A disposable `sshd -t`
run passed the `/var/empty` root-ownership precondition after its inode was
prepared offline, then timed out before a result marker. Its last diagnostic
included syscall 159; the target syscall header names 159 `setgroups`, and the
current dispatcher has no matching case. The call's relationship to the
timeout is not proven. No listener or transfer was established.

In a separate run, `scp` reached the local process setup and reported
`socketpair: Function not implemented` (ENOSYS); this remains a concrete
kernel/syscall compatibility blocker. The same test profile lacked configured
loopback and reported `Network unreachable` for SSH. The tested SCP source
still fails to compile because this target's `glob_t` lacks `gl_matchc`.
These conditions must be resolved and retested on a hash-pinned integrated
candidate before claiming source-transfer support.

### Corrected guest-side SSH/SCP retest (2026-09-30)

On a disposable ext4 copy, the hash-verified `ssh`, `sshd`, and pipe-based
`scp` listed above were run with a temporary RSA key and localhost-only
configuration. `/tmp/rem-lo-up` successfully configured `lo` as `127.0.0.1`
using `SIOCSIFADDR`/`SIOCSIFFLAGS`; it printed `LO_UP_PASS`. `sshd -t -f
/tmp/sshd_config` returned 0. The config used port 22222, host key
`/tmp/host_key`, authorized keys `/tmp/authorized_keys`, root public-key
authentication, `UseDNS no`, and `Subsystem sftp internal-sftp`.

The interactive client then reached `connect(127.0.0.1:22222)` and received
`ECONNREFUSED`. `scp` executed its pipe-based child `ssh` in SFTP mode in both
push and pull attempts; each ended with `Connection refused` and exit 255, and
no file was transferred. A foreground `sshd -ddd -e` reached version and
configuration parsing but produced no “Server listening” message before the
30-second host timeout. Thus the earlier “Network unreachable” was resolved
by bringing up loopback, but a functioning SSH listener and authentication
were not established.

The tested kernel artifact
`e2b7fe6672ee1e37b849815e6fc263844036c44c0255d85a10f67251a15041d1` logged
unknown syscall 159 (`setgroups`) during `sshd -t` and startup, unknown 233
(`madvise`) after foreground `sshd` parsed its config, and repeated unknown 407
(`clock_nanosleep_time64`). The current shared dispatcher source already has a
`setgroups` case, so the observed 159 failure establishes a source/binary
tuple mismatch for this test kernel. The dispatcher has no observed handling
for 233 or 407. These syscall failures coincide with the startup stall, but
the trace does not prove which one prevents the listener; classify the
remaining cause as a kernel/runtime compatibility blocker pending a kernel
owner retest. The client also produced one `MYEMU_USER_ALIGN`/`Bus error`
variation; another run consistently reached `ECONNREFUSED`, so that fault is
not attributed to the connection refusal.

The rootfs copy also needed `/var/empty` uid/gid corrected to 0 for OpenSSH's
server-directory precondition. The tested network kernel lacks the `fchownat`
dispatcher entry (syscall 54), so this was corrected offline in the disposable
image only. `sshd` subsequently passed `-t`. ext4 page-buffer warnings were
also logged during guest writes; no causal link to the SSH listener failure
was established.

The test tuple was QEMU
`34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`, the
kernel hash above, and disposable rootfs SHA-256
`dd3a8f338ec774ad8260449ed7cd8a6eaa7a1a993dd9230a7a5da977d236a02f` (with
`/var/empty` fixed and a test loopback helper staged). No production image was
changed. QEMU PID 4 in its isolated process namespace was reaped after each
run; the final host process check found no `qemu-system-myemulator32` process.

For a repeatable integration attempt, #4 needs: the matching userspace/libc
and a kernel rebuilt from the current dispatcher (including syscall 159),
validated `madvise` and `clock_nanosleep_time64` handling or proven safe
fallbacks, `/var/empty` owned by root and mode 0755, `/etc/passwd` entry for
the privilege-separation account if required by this OpenSSH build, host keys
with restrictive permissions, authorized keys, writable `/tmp`, and the
`internal-sftp` subsystem. Configure and raise loopback before localhost
tests; for host transfers, also configure the guest NIC and host-side TCP port
forwarding/SSH reachability. Retest listener readiness first, then SSH auth,
then SCP push and pull with content/hash checks.

### Startup-path inspection and helper preparation (2026-09-30 continuation)

Source inspection of `openssh-headerfix-rebuild/sshd.c` shows the pre-listener
order:

1. `main()` calls `setgroups(0, NULL)` before descriptor sanitization.
2. It parses options, opens/duplicates `/dev/null`, seeds the RNG, loads the
   config, then loads and shields host keys.
3. For a root launch it validates the `sshd` account and `/var/empty`.
4. Outside `-t` mode, it calls `setgroups()` again and stats the configured
   `/usr/libexec/sshd-session` and `/usr/libexec/sshd-auth` paths.
5. It sets logging/umask, changes to `/`, then calls `platform_pre_listen()`
   and `server_listen()`. `server_listen()` resolves listen addresses and
   calls `socket`, `fcntl`, socket-option setup, `bind`, and `listen`; only
   then does OpenSSH log “Server listening”.

The captured `sshd -ddd -e` output reached version reporting and finished
parsing `/tmp/sshd_config`, but stopped before a host-key fingerprint, helper
path diagnostic, or “Server listening”. The kernel logged unknown 233
(`madvise`) and 407 (`clock_nanosleep_time64`) in this interval. The trace
does not identify which call stalls. Syscall 159 (`setgroups`) is explicitly
called by this startup path and was logged as unknown by the tested kernel.
The `-t` check is insufficient as a listener test: source exits after config
and privilege-separation directory checks, before checking helper binaries or
opening a socket.

Inspection of the tested rootfs also found `/usr/libexec/sshd-session`
present (readback hash
`d80e39f1784793153feaf5c402aba14e3b188f42e85f75fc877f248aee5c91b1`) but
`/usr/libexec/sshd-auth` absent. The latter is a required pre-listener check
in this OpenSSH build. I built it with the existing configured command
`make -C /tmp/rem-openssh-current/openssh-headerfix-rebuild -j4 sshd-auth`;
its SHA-256 is
`5f83fa43aae36879f1a0d954ab407f3b85ebadd4eafa06fcb111e1650671f944`.
It was staged root:root mode 0755 into a separate disposable image at
`/tmp/rem-ssh-codex5-20260930/openssh-candidate-with-auth.ext4`; extracted
readback matches the build and `e2fsck -fn` passed. The helper is not yet
qualified on a replacement kernel.

Replacement-kernel quick gate: verify QEMU/kernel/rootfs hashes; boot the
`with-auth` image with the networking-qualified NIC path; configure loopback
and guest networking; run `sshd -t`; start `sshd -ddd -e` and require the
“Server listening” line; then authenticate with `ssh localhost`. Only after
that, test host-to-guest SCP and guest-to-host SCP, compare content hashes,
repeat the transfers, and reboot the same filesystem to verify persistence.
The retest runner and QEMU records are under
`/tmp/rem-ssh-codex5-20260930/`; do not substitute the older kernel or claim
the sequence passed based only on `sshd -t`.

### Pre-listener runtime call inventory and replacement-kernel gate (2026-09-30)

Further source inspection confirms these relevant operations occur before
`server_listen()` can print “Server listening”: `setgroups`; `closefrom`
(using the configured proc-fd or descriptor-loop fallback); open/dup of
`/dev/null`; `getrandom` or `/dev/urandom` reads in RNG seeding; config and
host-key file reads; `uname`; passwd lookup for `sshd`; `stat` of
`/var/empty`, `sshd-session`, and `sshd-auth`; `umask`; daemon/session checks;
`chdir("/")`; signal setup; address resolution; then socket creation, fcntl,
socket options, bind, and listen. The explicit source-level `setgroups()` is
evidence that syscall 159 is required for this server startup path. The
previous guest trace also logged 233 (`madvise`) and 407
(`clock_nanosleep_time64`), but did not map either log to one specific source
operation. A full syscall trace is still needed to establish their caller and
whether they are fatal to listener creation.

An initial timestamp-only assessment incorrectly rejected
`/tmp/rem-codex1-kernel-build/vmlinux`. Its SHA-256 is
`d549b69ea880e0270ada0ca132ed45ec94abbe27b362b13b9ee1bc6fb62a9032`; the
dispatcher source in its source tree has the same SHA-256 as the current
repository source (`b8ac8e951ba9f9286ee404db301a66f6704eb9ebba303310db758e37050fadfe`).
Disassembly of `myemulator2_syscall` in the linked kernel confirms syscall
233 branches to `sys_madvise`, syscall 407 branches to `sys_clock_nanosleep`,
and the 159 path reaches `sys_setgroups` (158 is `getgroups`). Its `.config`
has `CONFIG_NET`, `CONFIG_INET`, `CONFIG_VIRTIO`, `CONFIG_VIRTIO_MMIO`,
`CONFIG_VIRTIO_NET`, and `CONFIG_EXT4_FS` enabled. This makes it a valid
replacement-kernel **test candidate**; it is not runtime-qualified yet. The
separate rebuild log that ended during `syncconfig` belongs to a later,
incomplete rebuild attempt and does not invalidate the existing linked
candidate. Next run the prepared `with-auth` disposable rootfs against this
binary and continue from the listener gate above.

### Machine command-line boot gate (2026-09-30)

Tested the current #1 network kernel
`/tmp/rem-codex1-network-build/vmlinux` (SHA-256
`e92a67d30fd64ae7d08be1caa77f2cf91e1e021969fb3541eff2c78282b21e6c`) with
the disposable SSH filesystem (SHA-256
`d271ac52fef65d962150514f78521a90f61c73848a2baa5d3103488ce70e7e28`) and
QEMU SHA-256
`34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`.
QEMU accepted the machine property
`-M myemulator32,append=root=/dev/myemu0 rootfstype=ext4 rw init=/sbin/init ...`
and entered the kernel. The kernel nevertheless behaved as if its command
line were empty: `/dev/root: Can't open blockdev`, then
`VFS: Cannot open root device "" or unknown-block(0,0): error -6`; the
partition list did show `myemu0` (major 103, minor 0), followed by a panic
before ext4 mount and `/sbin/init`. Thus this run did not reach any SSH test.
The boot log is `/tmp/rem-ssh-codex5-20260930/append-test-serial.log`; the
QEMU record is `machine-append-test.json` in the same directory.

This is an observed kernel/QEMU boot-protocol blocker, not an OpenSSH result.
The live build directory `.config` currently says `CONFIG_CMDLINE_FORCE` is
unset and contains a built-in command line, which conflicts with the empty
command-line runtime result and may not describe the exact linked binary.
Kernel ownership should verify the linked image's command-line configuration
and the QEMU-to-kernel boot handoff. Do not proceed to SSH qualification on
this tuple until the kernel mounts the test rootfs and reaches init.

The full runner also could not start its temporary host-side `sshd` in this
managed environment: bind returned `Operation not permitted` before QEMU
launch. No network-enabled SCP result was obtained. QEMU launched for the
isolated append-property test was stopped/reaped after the pre-init panic;
the host-visible `ps -C qemu-system-myemulator32` check was empty.

### SSH/SCP retest with #1 current kernel (2026-09-30)

Correction to the previous boot-gate result: that run used the older flight
QEMU binary (SHA-256 `34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85`).
#1's working invocation uses repository `.qemu-build/qemu-system-myemulator32`
(SHA-256 `4e3b87b318f543b64f20eeb1d022335faaede529c245f4329c8617781e26f8b9`)
and `-M myemulator32,append=console=ttyMY0 115200 root=/dev/myemu0 rw
init=/sbin/init`. Retrying with that binary and argument form logged the
command line, mounted ext4, and ran `/sbin/init`. This corrected the runner's
QEMU tuple; no kernel command-line fix was needed. For NIC discovery, the
working appended line also includes
`virtio_mmio.device=0x200@0xf0200000:5` (the machine's single virtio-net
MMIO device).

On that tuple (`e92a67d30fd64ae7d08be1caa77f2cf91e1e021969fb3541eff2c78282b21e6c`
kernel, current `.qemu-build` QEMU, and the disposable qualification rootfs),
`/tmp/rem-lo-up` returned `LO_UP_PASS`; `/tmp/eth-up` returned
`ETH_UP_PASS 10.0.2.15/24 via 10.0.2.2`. `sshd -t -f /tmp/sshd_config`
returned 0, and `sshd -D -ddd -e` printed
`Server listening on 127.0.0.1 port 22222.` The guest SSH client then logged
`Connection established` to `127.0.0.1:22222`, but the server immediately
closed before key exchange. The server debug log identifies the failure as
`reexec socketpair: Function not implemented`. This is a syscall/kernel
compatibility blocker after TCP connect, before authentication; authentication
and both SCP directions remain untested.

The current dispatcher has `MYEMU2_NR_SOCKET 198` and a `sys_socket` case in
`linux/arch/myemulator2/kernel/syscall.c`, but no socketpair handler. In the
REM asm-generic ABI, `socketpair` is syscall 199. Kernel owner #1 should add
and validate this syscall (including returned descriptor pair semantics);
then rerun localhost SSH before cross-host SCP. The exact guest/QEMU command,
serial trace, and syscall evidence are in
`/tmp/rem-ssh-codex5-20260930/ssh-vvv-retry.json` and
`sshd-ps-retry.json`. The prepared end-to-end runner now points at
`.qemu-build/qemu-system-myemulator32` and the corrected append string.

One earlier listener probe used guest `grep` to inspect the daemon log and
triggered `MYEMU_BAD_USER_FAULT pid=39 comm=grep`; that harness check was
removed. The later debug-log capture used `cat` and reached the explicit
socketpair error above. The disposable image's BusyBox lacks `ifconfig` and
`route` applets, so network setup and displayed status currently rely on the
small `/tmp` helpers. Host-to-guest forwarding and guest-to-host SSH were not
attempted in this retest; this sandbox denied host socket binds with
`Operation not permitted`.
