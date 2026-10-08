# Actual Fold private musl baseline — 2026-10-08

## Exact input

The byte-for-byte archive supplied from the actual Fold is
`/tmp/libc.a.fold`, size 3,276,828 bytes, SHA-256
`0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186`.
The file was rechecked before this qualification. This is the baseline at
`/home/dev/rem-native-userland/bootstrap/musl/native/libc.a`; it is distinct
from `/usr/lib/libc.a`.

Only the archive bytes were supplied from the phone; its inode ownership and
mode were not included in that copy. The qualification image was based on the
retained stopped Fold simulation and the exact archive bytes were injected at
the private path. The host regression uses the retained image's mode `0644`,
uid `1000`, gid `100` metadata for its fixture. On the phone, the installer
records the actual target inode metadata and rollback restores those exact
values.

## Member and object comparison

The occurrence-aware inventory in `/tmp/rem-libc-identify.g59nt0n9/` records
all members, including duplicate names. The Fold archive has 1,346 ELF object
members and the two normal archive index/name-table members. Every object is
ELF32, little-endian, relocatable, REM machine `0xf2e2`. Duplicate occurrences
of `clone.o`, `free.o` and `realloc.o` were kept distinct.

Against the known original flight runtime (`30cff688...`, 2,835,504 bytes),
1,312 object occurrences are byte-identical. Exactly 33 object occurrences
differ, with no removed objects; the only addition is `native_syscall.o`.
Those 33 names are exactly the 33 translation units in the recovered full
bootstrap recipe at `toolchain/native-userland/patches/musl-rem-native.patch`
(`build-native.sh`, `full` case). There are no unexpected changed names:

```text
__rem_pio2_large.o catopen.o crypt_blowfish.o crypt_sha512.o cuserid.o
faccessat.o fcvt.o floatscan.o fputws.o getaddrinfo.o gethostbyname2_r.o
getnameinfo.o glob.o lookup_name.o lookup_serv.o mbsnrtowcs.o memmem.o
netlink.o nftw.o posix_spawn.o realpath.o res_send.o resolvconf.o sem_open.o
sigset.o strstr.o strtod.o syslog.o system.o tempnam.o timer_create.o
vfprintf.o wcsftime.o
```

The added `native_syscall.o` has SHA-256
`8d069eb08107843b771e362e662108d06cbedaac00287a1a78bd5c543d3c5cc2`.
It is byte-identical to the object reproduced from the recipe's REM syscall
assembly using the retained REM assembler. The comparison artifacts also
record no removed exports; the added syscall/atomic exports match the native
musl overlay. Five new external references (`__ashldi3`, `__eqsf2`,
`__lesf2`, `__lshrdi3`, `__ltsf2`) occur in changed objects. The update does
not carry any of these objects forward.

Against the corrected qualified runtime (`269b1d6b...`, 2,861,972 bytes), the
Fold archive is not byte-equivalent: 1,300 object occurrences differ. That is
why the update replaces the **entire** private archive with the exact
corrected qualified payload. No member from the Fold archive is merged into
the new archive. The exact Fold archive remains backed up byte-for-byte for
rollback. Installer acceptance is limited to the exact hash above at the one
private bootstrap path; the same bytes at `/usr/lib/libc.a` remain unsupported.

## Qualification condition

The object comparison identifies the Fold-specific differences as the
bootstrap recipe's replacement objects plus its syscall object. It does not
claim those Fold-built C objects are independently safe to link. Safety here
depends on installing the qualified replacement in full, confirming the
original archive backup and rollback, and running the complete compiler,
self-rebuild, Samurai and update qualification on an image containing these
exact input bytes. The package is not qualified or transferable unless all
required REM markers are present and the exact final archive passes the
installer regression.

Raw occurrence inventories, object/symbol comparison JSON, disassembly,
reproduced syscall object and the Fold object extraction are retained in
`/tmp/rem-libc-identify.g59nt0n9/` for this session. The exact original input
is retained at `/tmp/libc.a.fold`.
