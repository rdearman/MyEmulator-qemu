# MyFS v1

MyFS is the deliberately simple read-only filesystem used by the MyEmulator
1.0 bootloader and `COMMAND.COM`. The host creates raw images; guest software
only reads files. There are no directories, allocation maps, permissions,
timestamps, or guest write operations.

All values are little-endian and all allocation units are 256-byte floppy
sectors.

## Layout

| Sector | Contents |
| ---: | --- |
| 0 | independent MyEmulator boot sector |
| 1 | MyFS header |
| 2 | fixed 16-entry directory |
| 3 onward | contiguous file data |

The header is 256 bytes. Its defined fields are:

| Offset | Size | Meaning |
| ---: | ---: | --- |
| 0 | 4 | ASCII `MYFS` signature |
| 4 | 1 | version, currently `1` |
| 5 | 1 | header size, currently `16` |
| 6 | 2 | directory start sector, currently `2` |
| 8 | 1 | directory length in sectors, currently `1` |
| 9 | 1 | directory entry size, currently `16` |
| 10 | 1 | directory entry capacity, currently `16` |
| 11 | 1 | reserved, zero |
| 12 | 2 | data start sector, currently `3` |
| 14 | 2 | total image sectors |

Each directory entry is 16 bytes. An entry with flags byte zero is unused;
an active entry has flags byte one.

| Offset | Size | Meaning |
| ---: | ---: | --- |
| 0 | 11 | canonical upper-case 8.3 name: eight base bytes plus three extension bytes, space padded |
| 11 | 1 | flags |
| 12 | 2 | first data sector |
| 14 | 2 | exact file size in bytes |

Files occupy `ceil(size / 256)` contiguous sectors. A zero-byte file occupies
one sector but retains size zero. The builder currently accepts up to sixteen
files, 16-bit sector numbers, and 16-bit individual file sizes. The normal
demonstration image is 5760 sectors (1.44 MiB).

## Building an image

The image builder requires no loopback or host filesystem support:

```sh
./tools/mkmyfs --boot boot.bin --output myemulator.img \
  --file COMMAND.COM=command.com.bin \
  --file HELLO.COM=hello.com.bin \
  --file README.TXT=README.TXT
```

Names are validated as DOS-style 8.3 names and are compared case-insensitively
by the shell. The boot sector must be at most 256 bytes.

The boot sector is loaded by RIKMON at `0x0200`. It reads the header and
directory, finds `COMMAND.COM` by name, loads it at `0x0400`, and jumps there.
The shell uses `0x0300-0x03ff` as its sector buffer and `0xe000-0xe1ff` for
its input/name/filesystem workspace. Transient `.COM` programs load at
`0x2000`.

## `.COM` convention

MyEmulator `.COM` files are raw instruction/data bytes with no executable
header. They load at `0x2000` and begin executing there. The initial SP is the
firmware-provided `0xf000`. `RUN` enters a program with `JLA`, so a program
returns to `COMMAND.COM` by executing `RET`. Programs must not overwrite the
shell at `0x0400`, the filesystem buffer at `0x0300`, or workspace at
`0xe000-0xe1ff`.

## Demonstration build

```sh
make myfs-demo
```

This creates `build/myfs/myemulator.img`, the boot sector, shell and sample
program. The generated development kernel runs the sector-0 loader directly:

```sh
tools/mkmyfs-kernel build/myfs/boot.bin build/myfs/kernel.bin
.qemu-build/qemu-system-myemulator -M myemulator -nographic \
  -kernel build/myfs/kernel.bin \
  -drive file=build/myfs/myemulator.img,format=raw,if=none,id=myemulator-floppy \
  -monitor none -serial none -chardev stdio,id=console
```

The checked-in absolute sources are assembled at their runtime addresses:
`myfs/boot.s` at `0x0200`, `myfs/command.s` at `0x0400`, and `myfs/hello.s`
at `0x2000`. The Makefile extracts those ranges into raw files.
