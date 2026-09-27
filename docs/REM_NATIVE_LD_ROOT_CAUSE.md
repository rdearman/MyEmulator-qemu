# Native REM linker path diagnosis

## Result

The selected self-hosted native linker passes the user's smallest required
case. The SIGSEGV is reproducible with a different, older linker that remains
installed under /usr.

| Guest executable | SHA-256 | Result on same REM hello object |
| --- | --- | --- |
| /root/native/bin/ld | edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65 | ld -r PASS |
| /usr/myemulator2-linux-musl/bin/ld (same file as old ld.bfd) | c93c1c868944f173341cf7166109c4c47a24559f33abbff788e9c00ff3379f3b | SIGSEGV, 139 |

In a single pinned guest run, native as assembled the empty object and
ld-hello.S. The /root/native linker successfully ran:

    ld -r ld-empty.o -o ld-empty-rel.o
    ld -r ld-hello.o -o ld-hello-rel.o
    ld -T /root/native/lib/myemulator2-user.ld -o ld-hello ld-hello.o

Native readelf identified the relocatable output as REM ELF32. The executable
printed REM NATIVE LINK PASS and exited zero. Native readelf and objdump also
inspected this linker's output. These checks use the REM-native ELF32 ld
executable, not the x86-64 host cross-linker.

The legacy /usr linker returned 139 on ld -r for the exact same object. The
kernel recorded:

    MYEMU_BAD_USER_FAULT comm=ld pc=020009bc addr=00001b70 cause=7 fp=00001b60

Host-side disassembly of that exact binary maps the instruction to
lw r2,16(r15), immediately after a call to lang_final; r15 is the invalid
low frame address 0x1b60. This indicates a user-space call/frame-state fault.
The link made it well past program entry and substantial BFD/link-state work,
but the output file was not created. The failure is not an early inability to
open or identify the ELF object.

## Build comparison and likely cause

Both binaries identify GCC 15.2.0 and -O2 in available metadata. Therefore
the compiler version string is insufficient to identify the build. DWARF
points to two different GCC source/build trees:

- Failing legacy linker: REM-emacs/.toolchain-build/gcc.
- Passing selected linker: MyEmulator-qemu/.rem-flight-build-20260924/hosted-gcc-build.

The MyEmulator2 GCC backend header hashes differ:

- Legacy: 07f7ae53173e37c52a0021f57a8de16eb03508f33845bf0e2c7f865313e3cde1.
- Selected: 7f4a150b49a7383582b063de040a4dc6179cb84b3619586a6b04dbb77387d432.

The selected backend contains later frame/outgoing-argument sizing and
save/restore changes. The evidence strongly points to stale compiler-generated
frame/call code in the old ld executable. The precise earlier call that first
corrupts r15 has not been isolated, so this is a likely class of root cause,
not a proven single-function GCC defect.

The selected linker configuration is recorded in
.rem-flight-build-20260924/native-binutils-build/ld/config.status: build
machine x86_64-pc-linux-gnu, host and target myemulator2-linux-musl, compiler
.rem-flight-build-20260924/hosted-gcc-prefix/bin/myemulator2-elf-gcc (GCC
15.2.0), REM sysroot, and YACC=bison -y. Binutils' generated linker grammar
is in the build, but the passing empty/hello -r cases rule it out as the
failure boundary of the selected linker.

## Flight integration requirement

The native linker is available and has an exact successful regression. Keep
/root/native/bin/ld at the recorded hash and make the native build environment
select /root/native/bin before /usr/bin. The old /usr copy is not a fallback:
unqualified ld may resolve to it and reproduce the crash. This work did not
modify the flight image; final path/image integration remains with its owner.

The independent binutils suite is not wholly accepted: ar, ranlib, and
readelf-on-archive still fault and are tracked in
REM_NATIVE_BINUTILS_QUALIFICATION.md.
