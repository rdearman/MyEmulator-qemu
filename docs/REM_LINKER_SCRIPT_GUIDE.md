# MyEmulator2 userspace linker script

The supported static userspace link uses
`toolchain/userspace/myemulator2-user.ld`. It sets `_start` as the entry,
emits ELF32 little-endian MyEmulator2 output, and places the first loadable
segment at `0x02000000 + SIZEOF_HEADERS`. Including `FILEHDR PHDRS` makes the
program-header table available to musl startup through `AT_PHDR`.

The script keeps text, read-only data, TLS, initialisation arrays, data, GOT
and BSS in the single initial load segment, and emits a `PT_TLS` and
`PT_GNU_STACK` header. This is deliberately a small static-link layout, not a
dynamic-loader contract. The linker may warn that the load segment is RWX;
that warning reflects the current compact script and should not be silently
treated as a hardened production layout.

Inspect a linked image with:

```sh
myemulator2-elf-readelf -l -h hello
myemulator2-elf-objdump -h -d hello
```

Do not replace the script with `-Ttext` merely to make an address look tidy:
the ELF headers must remain covered by a load segment for the current musl
startup path.
