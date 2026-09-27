# REM native binutils overlay

This rootfs overlay installs the verified REM-native Binutils artifacts at
`/root/native/bin/`. The `usr/bin/` and
`usr/myemulator2-linux-musl/bin/` paths are compatibility symlinks to those
canonical binaries. `ld.bfd` aliases the verified `ld`; the stale linker is
not included.

Merge this directory into the final guest root filesystem preserving
permissions and symlinks (for example, `cp -a overlay/. ROOTFS/`). Put
`/root/native/bin` first on PATH. The overlay intentionally contains only
binutils tools; it does not replace chibicc, Samurai, the kernel, or image
contents.

Validated tools are `as`, `ld`, `ar`, `ranlib`, `objdump`, `readelf`, `nm`,
`size`, and `strings`. `objdump`, `nm`, `size`, and `strings` now come from
the existing REM-native Binutils build tree; guest smoke used a newly linked
small REM executable, with `nm`, `size`, and `strings` also run on
`/usr/bin/true`. The older overlay `objdump` and `nm` both faulted on
`/usr/bin/true`; the replacement candidates passed on the small executable.
Full disassembly of `/usr/bin/true` exceeded one minute under instruction
counting and was stopped, so large-file performance is not claimed.

`objcopy` remains from the earlier overlay; its exact-image version probe
passed, but file transformation was not tested in this run. The previous
overlay `strip` faulted at PC `0x0200701c`, address `0x2040`, while stripping
a REM executable. The replacement `strip-new` passed on a newly linked
executable, and the stripped result remained readable by `readelf -h`. The
tools report Binutils `2.46.0.20260210`. The candidate image was
built from the hash-verified rootfs copy
`REM-FLIGHT-ANDROID-MVP/rootfs/rem-rootfs.ext4`; it booted with ext4 and
devtmpfs mounted and an ash prompt. The candidate is disposable and does not
modify `REM-FLIGHT-TTY`.

`SHA256SUMS` records the canonical executable hashes. `MANIFEST.txt` records
the canonical destinations and compatibility symlinks.
