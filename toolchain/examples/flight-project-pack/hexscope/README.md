# hexscope

A byte-oriented file inspector. Its normal mode prints stable 16-byte rows
with offsets and an ASCII column; `--elf32` decodes the portable ELF32 file
header fields without mapping a C struct over untrusted bytes. The latter
works for either byte order and reports numeric machine IDs, including custom
targets. It rejects truncated or non-ELF32 headers before printing fields.

Offsets remain correct across the 32-bit boundary: rows below 4 GiB use an
8-digit offset, while later rows use a 16-digit high/low offset.

## Build and use

The project deliberately has multiple C translation units:

```sh
./build.sh
./hexscope samples/short.txt
./hexscope --elf32 ./hexscope
```

`build.sh` invokes the matching native `chibicc` driver for each `.c` file
and links them. It does not require GNU Make. With accepted Samurai, run
`samu -f build.ninja` instead. Native `as` and `ld` are selected by the
configured chibicc driver; they must match the compiler/runtime tuple.
For an x86/ARM Linux host build, use `CC=cc ./build.sh`.

## Expected output

For the included `REM flight\n` sample:

```text
00000000  52 45 4d 20 66 6c 69 67  68 74 0a                |REM flight.|
```

For ELF32, the output begins with `ELF32 little-endian` or
`ELF32 big-endian` followed by numeric `type`, `machine`, `entry`, `phoff`,
and `shoff` fields. Values vary with the input executable. This decoder does
not claim ELF64 support.
