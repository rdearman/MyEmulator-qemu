# REM User Guide

This is the practical offline guide for a REM development session.

## Booting and login

**VERIFIED:** Boot REM with the supplied launcher and wait for the `REM#` prompt. The console is the REM serial terminal. A visible prompt is useful only when input is also accepted; if input does not execute, stop and check the launcher/rootfs console wiring.

The preferred interactive shell is Yash when it is installed:

```sh
/usr/bin/yash
```

BusyBox `ash` remains the recovery shell. Exit Yash with `exit` to return to its parent shell.

## Filesystem layout

Common locations are:

```text
/bin                 BusyBox and basic recovery commands
/usr/bin             installed user programs and compatibility tools
/root                working files and flight projects
/root/native/bin     authoritative REM chibicc/binutils toolchain
/root/native/lib     linker script and compiler runtime archives
/usr/lib             CRT objects and musl static libraries
/tmp                 disposable working files
/proc, /sys          kernel process/device information when mounted
```

Keep source and important results under `/root` or another persistent directory. Use `/tmp` for scratch files.

## A normal session

```sh
/usr/bin/yash
cd /root
kilo hello.s
```

In Kilo, edit the file, save it, and quit. Then assemble and link it with the installed toolchain. The exact tool locations can be made explicit:

```sh
export PATH=/root/native/bin:/usr/bin:/bin:/usr/sbin:/sbin
export CHIBICC_REM_AS=/root/native/bin/as
export CHIBICC_REM_LD=/root/native/bin/ld
export CHIBICC_REM_LINKER_SCRIPT=/root/native/lib/myemulator2-user.ld
export CHIBICC_REM_CRTDIR=/usr/lib
export CHIBICC_REM_GCCLIB=/root/native/lib
```

## Compiling a C program

**VERIFIED for the supported integer/pointer subset:**

```sh
chibicc -o hello hello.c
./hello
```

For an explicit staged flow:

```sh
chibicc -S -o hello.s hello.c
as -o hello.o hello.s
ld -T /root/native/lib/myemulator2-user.ld -o hello hello.o
./hello
```

A hosted C link may require `/usr/lib/libc.a`, CRT objects, and `/root/native/lib/libgcc.a`; keep matching runtime files together.

## Building projects

**VERIFIED:** Direct command scripts and native assembler/linker workflows are the most predictable bootstrap method. **PLANNED:** use Samurai with a project build file once the integrated image contains its validated executable and matching compiler/binutils tuple.

Before a build, confirm that `chibicc`, `as`, and `ld` resolve from the authoritative path. Inspect output with `readelf` and `objdump` when diagnosing a failed link.

## Reboot, shutdown, and persistence

Flush important writes before leaving:

```sh
sync
```

Use the image's documented shutdown command if provided, then exit the shell. A clean shutdown reduces ext4 recovery on the next boot. **PARTIAL:** ext4 file persistence has passed focused disposable checks, but the complete real developer workload has not yet passed a clean shutdown, reboot, and rebuild cycle. Always verify a critical file after reboot:

```sh
cat /root/projects/hello.s
```

Do not treat a forced QEMU timeout as a clean shutdown. **FUTURE:** a single polished power-off command and image-integrity check for every launcher.
