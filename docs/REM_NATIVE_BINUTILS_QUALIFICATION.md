# Native REM binutils qualification

**Authoritative tested linker:** `/root/native/bin/ld`, SHA-256
`edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65`.
It passes relocatable linking, executable linking, and execution in the pinned
REM guest. `/usr/myemulator2-linux-musl/bin/ld` is a different, stale binary
and crashes on `ld -r`; do not use it.

The tests use QEMU SHA-256
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`, kernel
SHA-256 `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`,
and source rootfs SHA-256
`46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd`.
Tests run on a disposable rootfs copy with the staged `ar` and `ranlib`
overlaid; the production flight image was not modified.

## Native archive tools

The old `ar` and `ranlib` crashed in libiberty `split_directories` while
storing through an invalid `ptr_num_dirs` pointer. For `ar`, the fault was at
PC `0x020e266c`, fault address `0x00000fa4`; for `ranlib`, PC
`0x020e0790`, the same fault address. In `ar`, the failing instruction was
`sw r5,0(r1)` after loading the pointer from the stack. The caller formed the
pointer from a bad frame value (`r15=0x1040`, yielding `0xfa4`) while the
actual stack pointer was near `0x3ff1a6a8`.

DWARF identifies the failing installed tools as built from the older
`REM-emacs/.toolchain-build/gcc` tree. Only `ar` and `ranlib` were rebuilt;
Binutils source was unchanged. The rebuild used the newer GCC 15.2.0
`xgcc` from `.rem-flight-build-20260924/hosted-gcc-build/build/gcc/` and the
existing native-binutils build tree. This isolates the defect to stale
compiler-generated frame/call code in the old utility binaries; it does not
prove a single GCC source-level defect.

The rebuilt candidates pass inside REM: archive creation and listing,
extraction, ranlib indexing, linking and running an archive-backed program,
then replacing one object, reindexing, relinking, and observing changed
output. `readelf` inspected an extracted archive member. The single-object,
multi-object, archive-backed executables and native readelf/objdump inspection
also passed after a synchronized QEMU stop/restart on that same disposable
image.

Rebuilt candidate hashes staged in `toolchain/flight-kit-runtime/bin/`:

| Tool | Bytes | SHA-256 |
| --- | ---: | --- |
| `ar` | 1,748,948 | `31585d2cfa5dcdf702e4655b2ac386b7580641aaa378d2b39bde106bb4543ba7` |
| `ranlib` | 1,748,960 | `b2f8ac9eba8000c34c1f609d1955bc8e3495532b3bf0739ad714554e36d00bd3` |

The focused repeatable regression is:

```sh
python3 toolchain/scripts/test-native-binutils-flight.py \
  --guest-script guest-archive-qualification.sh \
  --post-reboot-script guest-archive-post-reboot.sh \
  --output-dir /tmp/rem-native-binutils-qualification-codex6-archive-pass
```

Its recorded result is `PASS`; logs and the machine-readable report are in
that output directory. The native C fixture remains blocked because the
currently staged chibicc rejects `/usr/include/bits/alltypes.h:322`
(`typedef __builtin_va_list va_list;`, expected `,`). No chibicc change was
made. The production rootfs still contains the old `/usr` archive tools until
the image owner integrates these staged replacements.

## Other primary tool hashes

These tested artifact identities remain unchanged:

| Tool | Bytes | SHA-256 |
| --- | ---: | --- |
| `as` | 2,158,532 | `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93` |
| `/root/native/bin/ld` | 2,493,912 | `edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65` |
| `readelf` | 1,616,096 | `778d9e788068b2d8b78e85bfa2cf99c1c51f78873c20939a2a8bd91eb01b2eaa` |
| `objdump` | 2,223,244 | `b7547055d61a954aeaaeae41240f8ef1aee0ac0456cfa43b1cbe03958091fbdc` |

Native `readelf` and `objdump` pass representative object and executable
inspection, including after reboot. Direct `readelf` inspection of the test
archive container is not qualified; extraction followed by inspection of its
REM ELF member passes. No REM64 note was added because this is a stale
compiler-output provenance issue, not a new demonstrated architecture
requirement.

## Integration overlay

`toolchain/native-binutils-flight-overlay/` is the integration overlay for the
final rootfs; a tar package is at
`toolchain/native-binutils-flight-overlay.tar`. It puts the verified copies in
`/root/native/bin/`, with `/usr/bin/` and
`/usr/myemulator2-linux-musl/bin/` aliases resolving to those same files.
`ld.bfd` resolves to the verified `ld`, avoiding the stale staged linker.
`as`, `ld`, `ar`, `ranlib`, `objdump`, and `readelf` are the required primary
tools; `nm`, `objcopy`, and `strip` are included as additional native tools.

The overlay has a deterministic structural/hash check:

```sh
python3 toolchain/scripts/check-native-binutils-overlay.py
```

It passed after packaging, checking all executable hashes, ELF32 little-endian
identity, executable modes, alias targets, and archive contents. The guest
qualification above validates the six primary tools in REM; the final flight
rootfs still needs the overlay applied and its owner to run acceptance on that
assembled image. The host workspace cannot directly access `/root/native/bin`
(the path is permission restricted), so the overlay's rootfs location is
established by its layout and the recorded guest tests, not by reading that
host path.

## Inspection utility integration (2026-09-27)

The existing native Binutils 2.46.0.20260210 build tree supplied the updated
`objdump`, `nm-new`, `size`, `strings`, and `strip-new` executables. They are
REM ELF32 little-endian machine `0xf2e2` and are staged under
`tools/rem-flight-image/staging/binutils/`. The accepted candidate hashes are:

| Tool | Bytes | SHA-256 |
| --- | ---: | --- |
| `objdump` | 9,125,056 | `57f7062b4f14ea3edd6aa2f74f7d6ffab21dcce8cadcdc0d7730c9b05140fa67` |
| `nm` | 5,526,136 | `da0c0a6ecc28f79aaf6086778f1fe4083b780762fa134b99e5f72d04858d7d71` |
| `size` | 5,474,196 | `8f39adfc4aa7762e0a14eb6c63a7530cebcfdb47580f36d4bf6783f534b62690` |
| `strings` | 5,462,176 | `c0de7c8e36a7f4e0db77f5047371d80bab4a76f4b376a0e866a69e2f150358da` |
| `strip` | 6,227,488 | `7f7648eb96acd09584d336f3c751a8a9dc17a33f26db37edd8ac1e024d8fced9` |

The pinned rootfs path in the original desktop deployment changed to
`04eaef9b630891818ff5dfeaa4b525465da345057f606158eacae01f0245d906`, so the
builder refused it. The rootfs copy at
`REM-FLIGHT-ANDROID-MVP/rootfs/rem-rootfs.ext4` independently hashed to the
tuple's original `46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd`;
the binutils tuple now uses this byte-identical source. Candidate
`/tmp/rem-flight-binutils-final-20260927` passed build verification and booted
to ash with ext4 and devtmpfs mounted. Its rootfs SHA-256 is
`a107a3b637837a4673be28688b44824f8a7765c1b14e660e32143ac343cc5d12`.

Using the candidate tools, `/root/native/bin/as` assembled and `/root/native/bin/ld`
linked a small executable. `readelf -h`, `objdump -d`, `nm`, `size`, `strings`,
and `strip` passed on that executable; the stripped output still passed
`readelf -h`. `nm`, `size`, and `strings` also passed on `/usr/bin/true`, as
did `readelf -h`. The old overlay `objdump` faulted at PC `0x0218ff28`,
address `0x46c`; old `nm` faulted at PC `0x020e01b8`, address `0xfa4`; old
`strip` faulted at PC `0x0200701c`, address `0x2040`. Fault causes are not
established. The current native `objdump -d /usr/bin/true` exceeded one minute
under instruction-counted QEMU and was interrupted, so large-file performance
is open. These tools' tested scope is recorded narrowly; this is not a full
flight-image acceptance run.

The regenerated deterministic overlay passes
`python3 toolchain/scripts/check-native-binutils-overlay.py`, including
hashes, executable modes, ELF32 endianness, symlinks, and tar contents. The
qualified assembler and linker hashes remain unchanged. The full disposable
candidate manifest and host verification are under the candidate path above.
