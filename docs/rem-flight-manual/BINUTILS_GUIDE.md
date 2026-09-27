# REM Binutils Guide

The authoritative native tools are installed under `/root/native/bin` in the validated runtime tuple.

## `as`

`as` assembles REM GAS source into an ELF32 MyEmulator2 object:

```sh
as -o hello.o hello.s
```

## `ld`

`ld` combines objects, libraries, and the REM linker script into an executable:

```sh
ld -T /root/native/lib/myemulator2-user.ld -o hello hello.o
```

The linker script defines the REM user image layout and entry contract.

## `ar` and `ranlib`

`ar` creates static archives; `ranlib` creates or refreshes the archive index:

```sh
ar rcs libmath.a add.o sub.o
ranlib libmath.a
```

## `objdump`

Disassemble and inspect objects:

```sh
objdump -d hello.o
objdump -h hello
```

## `readelf`

Inspect ELF headers, sections, symbols, and program segments:

```sh
readelf -h hello
readelf -l hello
readelf -S hello
readelf -s hello
```

**VERIFIED:** the qualified native tuple supports assembly, linking, archive operations, executable inspection, and representative persistence checks. Always keep the tools from one tuple together; do not mix stale `/usr/bin` copies with `/root/native/bin`.
