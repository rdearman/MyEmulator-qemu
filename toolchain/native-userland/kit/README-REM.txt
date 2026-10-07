REM native userland source kit

Sources: repaired REM chibicc, musl replacements, LLVM compiler-rt 15.0.7,
Ryu (temporary bootstrap only), GNU Make 4.4.1, BearSSL 0.6, curl 8.12.1, coreutils 9.7,
findutils 4.10.0, grep 3.11, sed 4.9, gawk 5.3.1, diffutils 3.10, tar 1.35,
gzip 1.13, bzip2 1.0.8, xz 5.6.3, Dropbear 2024.86 SSH/SCP clients,
ncurses 6.5 and Vim 9.1.0000.
Upstream licenses are retained in their source trees.

Build on REM, using its installed chibicc, GNU as/ld, bash and Samurai:
    ./build-all-native.sh
    ./install-native.sh

No Python, CMake or Meson is used by the native scripts. A plain tar archive
needs no decompressor. Installation defaults to $HOME/.local; set REM_PREFIX
consistently for both build and installation to choose another prefix.
Add $HOME/.local/bin to PATH if it is not already there.

Build order: integer compiler, source-built IEEE helpers, temporary Ryu
compiler, source-built strtod seed, unrestricted compiler, repaired libc,
final compiler, compiler self-rebuild, configure bootstrap helpers, BearSSL,
HTTPS curl, coreutils, SSH clients and remaining GNU/compression
utilities. Every stage stops on failure. Logs are kept
under logs/. An acceptance marker is required before installation.
Bootstrap-only source helpers supply expr, dirname/basename and the rm/cp/wc
flags missing from the installed small utilities. They are not installed over
system tools; the completed GNU coreutils take precedence during later builds.

Coreutils builds the programs selected by its REM-target source configuration.
Coreutils, findutils, grep, sed, diffutils, tar, gzip, gawk, XZ, Dropbear,
curl, ncurses and Vim use supplied source-only Samurai graphs; they do not
run configure or Make on REM. Native qualification remains incomplete.
GNU Make is not a release gate.
Every requested package is included; package qualification is
tracked separately from merely supplying its source and build script.

Acceptance runs the existing IEEE and libc reducers, real cp/mv/rm/ln/ls/
mkdir/chmod/cat, find/grep/sed/awk/diff/tar/compression operations, raw TCP,
DNS/hostname TCP regressions, and a certificate-verified HTTPS download from
https://example.com/. A working network and correct guest clock are needed.
The bundled trust anchors are public Mozilla CA certificates; their license
notice is in certificates/COPYRIGHT. No insecure TLS bypass is used.

Installation preserves existing targets, including symbolic links, in
$REM_PREFIX/lib/rem-native-userland/backups/. Each file replacement is an
atomic same-directory rename. A failure attempts rollback. To roll back:
    bash rollback-native.sh
Do not interrupt installation/rollback or run a compiler build concurrently
with installation. Multi-file installation is not a single transaction.

The compiler wrapper selects private libc and IEEE runtime files. The system
libc archive, kernel, QEMU, BusyBox vi, init/getty, boot and rootfs images are
not replaced. No installer touches /sbin, /etc or /usr/lib.
Vim installs only as vim, with its runtime data and a terminal database under
the chosen prefix. It never installs vi/ex/view aliases or replaces BusyBox vi.

SSH is Dropbear dbclient installed as both dbclient and ssh; scp is its classic
SCP client. No SSH server is built or installed. It works with a remote SSH
server that permits the SCP protocol, for example:
    scp rick@100.70.35.84:/path/to/file .
dropbearkey/dropbearconvert support client key generation/conversion.
Host-key verification remains enabled. No credentials are in this source kit.
Every REM source delta is also recorded in patches/ against the pristine
upstream release (for example patches/vim-9.1.0000-rem-native.patch and
patches/diffutils-3.10-rem-native.patch); the package sources already include
those patches.
To run the isolated SCP regression, set `SSH_TEST_HOST`, `SSH_TEST_PORT`,
`SSH_TEST_USER`, `SSH_TEST_KEY`, `SSH_TEST_KNOWN_HOSTS`, and
`SSH_TEST_SOURCE` before `./build-all-native.sh`; the acceptance script uses
a pinned known_hosts file and compares the downloaded bytes with
tests/ssh-scp-payload.txt. Without those values it explicitly logs that SCP
acceptance was not run; this is not a qualification PASS.
The rebuilt nc supports the basic form "nc HOST PORT"; it is linked to the
repaired resolver instead of replacing the BusyBox executable itself.

Qualification status: the IEEE runtime passed its numerical regression;
the compiler rebuilt itself natively after the ILP32 literal/printf repair.
Raw TCP, DNS, hostname TCP and repeated resolver use pass in the disposable
guest. Repaired frame alignment, integer/Boolean bitfields, hexadecimal/pointer
formatting and a second complete compiler self-rebuild also pass natively.
The clean source-kit compiler/runtime bootstrap and compiler self-rebuild
completed. Installed GNU Make was OOM-killed before BearSSL compilation.
BearSSL now builds directly with a native shell script; GNU Make qualification
is not a release gate. Its optional source bootstrap remains included.
Curl/coreutils, SSH/SCP, Vim and installer acceptance remain pending.
This source tree is not yet an accepted release.
Later required source fixes add aggregate/alloca/VLA lowering, negative-array
constraint checks, hashmap collision handling and long conditional jumps.
Their newest native self-rebuild and regressions remain unqualified.
Curl no longer falsely selects unsupported atomic exchange; its synchronous
CLI retains HTTPS/BearSSL, but libcurl does not advertise thread-safe global
initialization. No non-atomic operation is presented as an atomic exchange.
All 226 curl C recipes emit REM assembly; that is not a native runtime PASS.
Vim's generated graph preserves normal terminal features and checks musl/
ncurses configuration contracts natively; optional unused annotations are
selected only when its actual callback declaration compiles.
Native qualification was interrupted by a kernel memcpy fault inside
copy_from_user_nofault. Its preceding userspace trigger is not established.
Evidence is preserved outside the kit; no kernel/QEMU change is included.
The Make configure timestamp-order check independently fails because the
installed bootstrap ls rejects -t; this is not proof that configure needs a
clock setter. CLOCK_REALTIME reports January 1970 plus elapsed time, and the
exposed setter/adjustment interfaces return ENOSYS. The probes are
tests/clock-settime-abi.c and tests/clock-interfaces.c. The existing REM bug
log documents the exact missing clock_settime64 interface and UTC requirement.
No certificate-time verification bypass, synthetic clock, kernel change or
QEMU change is provided. Independent package builds continue without Make;
installation refuses an unaccepted build.
The existing bug reports and recorded native results preserve detailed
evidence; pending work must not be inferred to have passed.
