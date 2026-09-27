# Bare-metal image recipe

Bare-metal ELF and raw firmware are different deliverables. ELF retains
symbols and program headers for loaders and inspection; a raw image contains
bytes at the platform's reset-image locations. Start with the repository
fixture and inspect it before changing the image format:

```sh
.toolchain-install/bin/myemulator2-elf-as \
  toolchain/examples/minimal.s -o /tmp/minimal.o
.toolchain-install/bin/myemulator2-elf-ld \
  /tmp/minimal.o -o /tmp/minimal.elf
.toolchain-install/bin/myemulator2-elf-readelf -h -l /tmp/minimal.elf
.toolchain-install/bin/myemulator2-elf-objcopy -O binary \
  /tmp/minimal.elf /tmp/minimal.bin
```

The reset/firmware loader used by a particular QEMU test supplies the image
at the platform-defined reset locations. Use the named test script or boot
recipe for that machine; do not infer a Linux userspace load address from a
bare-metal raw image. `tests/programs/README.md` and
`tests/run-assembler-qemu-test.sh` are the current fixture-oriented entry
points.
