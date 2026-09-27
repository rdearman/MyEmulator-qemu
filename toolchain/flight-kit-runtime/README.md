# Hosted-C flight runtime staging copy

This directory is a **copy for later image integration**, not a production
image and not a change to another agent's rootfs. It was assembled from the
disposable candidate guest tree and GCC runtime that produced the validated
native hosted-C tuple in `docs/REM_VALIDATED_RUNTIME_TUPLES.md`.

## Install mapping

Preserve the directory contents and modes when copying into a guest rootfs:

| Staging path | Guest destination |
|---|---|
| `bin/chibicc`, `bin/include/` | `/root/native/bin/` (keep compiler and sibling `include/` together) |
| `bin/as`, `bin/ld`, `bin/ar`, `bin/nm`, `bin/objcopy`, `bin/objdump`, `bin/ranlib`, `bin/readelf`, `bin/strip` | `/root/native/bin/` |
| `include/` | `/usr/include/` |
| `lib/crt1.o`, `lib/crti.o`, `lib/crtn.o`, `lib/libc.a` | `/usr/lib/` |
| `lib/libgcc.a`, `lib/myemulator2-user.ld` | `/root/native/lib/` |

With those paths installed, the native driver invocation is:

```sh
PATH=/root/native/bin:$PATH \
CHIBICC_REM_AS=/root/native/bin/as \
CHIBICC_REM_LD=/root/native/bin/ld \
CHIBICC_REM_LINKER_SCRIPT=/root/native/lib/myemulator2-user.ld \
CHIBICC_REM_CRTDIR=/usr/lib \
CHIBICC_REM_GCCLIB=/root/native/lib \
chibicc -o hello hello.c
./hello
```

For binutils, apply `toolchain/native-binutils-flight-overlay/` to the guest
rootfs after the runtime staging files. It installs one authoritative copy
of each tool in `/root/native/bin/` and compatibility symlinks from
`/usr/bin/` and `/usr/myemulator2-linux-musl/bin/`. Its `ld.bfd` symlink
targets the validated native `ld`; do not copy the stale staged `bin/ld.bfd`
over that alias. The packaged overlay is
`toolchain/native-binutils-flight-overlay.tar`.

The toolchain is validated for the integer/pointer C subset and hello link/run
recorded in the compiler status. This does not claim that all starter projects
or all C/POSIX features compile and run. Keep the image's QEMU, kernel, rootfs,
and these files as the exact runtime tuple.

## Provenance checks

These SHA-256 values match `docs/REM_VALIDATED_RUNTIME_TUPLES.md`:

```text
7b9134caaf776953d82d34c3f54dec0eb5b3f343d8ec92ff350df6ee82023a75  bin/chibicc
c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93  bin/as
edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65  bin/ld
9951b664ec350b8ae34c30be9824a42b3a76253978515d674bf1d33b346a4697  lib/crt1.o
7fb830f63e7ba115f433b6f28253e8510bbc7b0ab0708c62a2a0ac94aeba5f37  lib/crti.o
5594cc2bd0fa13d22e159c78c1f3065b7f36ae217badcdeaf8e71b5c3ad64937  lib/crtn.o
30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924  lib/libc.a
759fb011a7d4dfba4da8a96d17cc978485f2daf62c2ba4545180d4de40773044  lib/libgcc.a
6ec14dea3bda5ded2ef8b23ac1995d6b40857e2273ead8e095d57f25e8a54621  lib/myemulator2-user.ld
```

The musl headers come from that same candidate rootfs; the headers are not
individually listed in the runtime tuple table. The extra native binutils
utilities were copied from the same candidate tree but are not required by
the validated hello link. The actual flight image still needs integration
and an end-to-end check after these files are installed.
