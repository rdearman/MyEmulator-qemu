# REM offline development index

Start here when the guest has no network. This index points at the current
source tree and records what was actually validated; a file's presence alone
does not mean that feature passed on-device.

## I want to...

| Task | Start here | Then use |
|---|---|---|
| Learn the complete offline developer workflow | [`REM_DEVELOPER_GUIDE.md`](REM_DEVELOPER_GUIDE.md) | [`REM_OFFLINE_WORKFLOW.md`](REM_OFFLINE_WORKFLOW.md) and the linked component references |
| Run flight-image user acceptance | [`REM_FLIGHT_UAT_CHECKLIST.md`](REM_FLIGHT_UAT_CHECKLIST.md) | Record the exact image/tool hashes and attach per-test logs |
| Write and compile C | [`REM_C_COMPILER_STATUS.md`](REM_C_COMPILER_STATUS.md), then [`toolchain/examples/flight-kit/README.md`](../toolchain/examples/flight-kit/README.md) | [`REM_VALIDATED_RUNTIME_TUPLES.md`](REM_VALIDATED_RUNTIME_TUPLES.md) for the matching compiler, CRT, libraries and tools |
| Write MyEmulator2 assembly | [`MYEMULATOR2_ASSEMBLY_PROGRAMMING_MANUAL.pdf`](MYEMULATOR2_ASSEMBLY_PROGRAMMING_MANUAL.pdf), [`REM_ASSEMBLY_CHEAT_SHEET.pdf`](REM_ASSEMBLY_CHEAT_SHEET.pdf) | [`REM_ASSEMBLY_SYSCALL_COOKBOOK.md`](REM_ASSEMBLY_SYSCALL_COOKBOOK.md), `toolchain/examples/` |
| Understand architecture and instruction encoding | [`REM_ARCHITECTURE64.md`](REM_ARCHITECTURE64.md), [`REM_PROGRAMMERS_GUIDE.pdf`](REM_PROGRAMMERS_GUIDE.pdf) | Assembly manual; GAS and encoding reference PDFs |
| Understand the ABI | [`MYEMULATOR_2_ABI.md`](MYEMULATOR_2_ABI.md) | [`REM_ELF_Generic_ABI.pdf`](REM_ELF_Generic_ABI.pdf), compiler status for current subset limits |
| Find syscalls | [`REM_CURRENT_SYSCALL_REFERENCE.md`](REM_CURRENT_SYSCALL_REFERENCE.md) | [`REM_POSIX_Linux_Reference.md`](REM_POSIX_Linux_Reference.md) and cookbook; dispatcher presence is not a test result |
| Link a program | [`REM_LINKER_SCRIPT_GUIDE.md`](REM_LINKER_SCRIPT_GUIDE.md) | `toolchain/userspace/myemulator2-user.ld`, target CRT and libc |
| Build without GNU Make | `toolchain/examples/flight-kit/build.ninja.*` and `build.sh` | These are examples; see Samurai status before relying on `samu` |
| Use the terminal | [`REM_TERMINAL_CAPABILITY_MATRIX.md`](REM_TERMINAL_CAPABILITY_MATRIX.md) | [`VT220_Programmer_Reference_Manual.pdf`](VT220_Programmer_Reference_Manual.pdf) |
| Edit with Kilo / use Yash | [`REM_YASH_STATUS.md`](REM_YASH_STATUS.md) | In an image containing them, run `kilo FILE` or `/usr/bin/yash`; both are staged/acceptance dependent |
| Administer the guest / understand accounts | [`REM_SYSTEM_ADMIN.md`](REM_SYSTEM_ADMIN.md), [`REM_USER_MANAGEMENT.md`](REM_USER_MANAGEMENT.md) | [`REM_SOFTWARE_INVENTORY.md`](REM_SOFTWARE_INVENTORY.md) for image-specific contents |
| Understand init, services, and runlevels | [`REM_INIT_SYSTEM.md`](REM_INIT_SYSTEM.md), [`REM_CONFIGURATION.md`](REM_CONFIGURATION.md) | `toolchain/userspace/rem-sysv/` provides a BusyBox-init staging seed |
| Check image/package provenance | [`REM_BUILD_PROVENANCE.md`](REM_BUILD_PROVENANCE.md) | [`REM_PACKAGE_SYSTEM.md`](REM_PACKAGE_SYSTEM.md) |
| Understand the filesystem and documentation layout | [`REM_FILESYSTEM.md`](REM_FILESYSTEM.md), [`REM_DOCUMENTATION.md`](REM_DOCUMENTATION.md) | Staged target copies live under `/usr/share/doc/rem` |
| Set up the intended Yash environment | [`REM_YASH_PROFILE.md`](REM_YASH_PROFILE.md) | `toolchain/userspace/rem-skel/` contains the starter profile files; image integration is separate |
| Build and run a C program | [`REM_BUILD_WORKFLOW.md`](REM_BUILD_WORKFLOW.md), [`REM_C_PROGRAMMING.md`](REM_C_PROGRAMMING.md) | [`REM_VALIDATED_RUNTIME_TUPLES.md`](REM_VALIDATED_RUNTIME_TUPLES.md) |
| Work offline / recover a project | [`REM_OFFLINE_WORKFLOW.md`](REM_OFFLINE_WORKFLOW.md) | [`REM_SOURCE_BACKUP.md`](REM_SOURCE_BACKUP.md), [`REM_EXT4_PERSISTENCE.md`](REM_EXT4_PERSISTENCE.md) |
| Install/verify a local package | [`REM_PACKAGE_SYSTEM.md`](REM_PACKAGE_SYSTEM.md) | `tools/rempkg/` prototype; guest use is pending |
| Set up the developer environment | [`REM_DEVELOPER_ENVIRONMENT.md`](REM_DEVELOPER_ENVIRONMENT.md) | [`REM_SOFTWARE_INVENTORY.md`](REM_SOFTWARE_INVENTORY.md) |
| Look up a command quickly | [`REM_CHEATSHEET.md`](REM_CHEATSHEET.md) | Verify the command exists in the selected image first |
| Understand console, timer and platform devices | [`REM_PLATFORM.md`](REM_PLATFORM.md) | [`MYEMULATOR_2_LINUX_PORT.md`](MYEMULATOR_2_LINUX_PORT.md) and selected QEMU tuple |
| Debug a REM process | [`REM_DEBUGGER_USER_GUIDE.md`](REM_DEBUGGER_USER_GUIDE.md) | Practical `remdbg` workflow; [`REM_DEBUGGER.md`](REM_DEBUGGER.md) records implementation and qualification; [`REM_DEBUGGER_INTEGRATION.md`](REM_DEBUGGER_INTEGRATION.md) covers separate QMP and kernel fault-log interfaces |
| Debug an error or broken build | [`REM_QEMU_Debugging_Reference.md`](REM_QEMU_Debugging_Reference.md), [`REM_KNOWN_GOOD_FIXTURES.md`](REM_KNOWN_GOOD_FIXTURES.md) | [`REM_C_COMPILER_STATUS.md`](REM_C_COMPILER_STATUS.md), [`REM_SAMURAI_STATUS.md`](REM_SAMURAI_STATUS.md), target manifest |
| Rebuild REM itself | [`REM_OFFLINE_BUILD_MANIFEST.md`](REM_OFFLINE_BUILD_MANIFEST.md) | [`REM_BARE_METAL_IMAGE_RECIPE.md`](REM_BARE_METAL_IMAGE_RECIPE.md); this is workstation-scale work |

## What's actually in this snapshot

- Architecture/ABI: architecture and ABI notes plus architecture, assembler,
  ELF, GAS, linker, POSIX/Linux, terminal and QEMU-debugging references.
- C: a REM chibicc source port and host executable are in the repository. A
  validated native hosted-C run exists for one pinned runtime tuple. A
  separate, hash-matched copy of that tuple's guest compiler/runtime is now
  staged in `toolchain/flight-kit-runtime/` for image integration.
- Assembly: the matched guest-native `as`/`ld` are staged at
  `toolchain/flight-kit-runtime/bin/`. Host-executable cross-binutils and GCC
  artifacts under `.toolchain-install/` are separate; use the guest tools for
  programs that will run on REM.
- Hosted C runtime: use `toolchain/flight-kit-runtime/` as a matched snapshot
  (read its README for target install locations and hashes). The partial
  `.musl-build/` directory is not the authoritative source for this snapshot.
  Minilibc headers/source and `crt0-linux.S` also remain available for limited
  work.
- Editors and shell: source/status notes cover Kilo and Yash, but inclusion and
  acceptance depend on the selected flight image. Keep the project source and
  docs locally; networking is not a recovery path.

For the copy checklist run `tools/check-offline-flight-kit.sh`. It reports
missing required source/docs and separately reports unavailable optional
prebuilt images/toolchains.
