#!/usr/bin/env bash
set -u
ROOT=${1:-.}
required=(
  docs/Inside_REM_Edition3.pdf
  docs/REM_PROGRAMMERS_GUIDE.pdf
  docs/MYEMULATOR2_ASSEMBLY_PROGRAMMING_MANUAL.pdf
  docs/REM_ASSEMBLY_CHEAT_SHEET.pdf
  docs/REM_POSIX_Linux_Reference.pdf
  docs/REM_GAS_Assembler_Reference.pdf
  docs/REM_GNU_LD_Linker_Reference.pdf
  docs/REM_ELF_Generic_ABI.pdf
  docs/REM_QEMU_Debugging_Reference.pdf
  docs/VT220_Programmer_Reference_Manual.pdf
  docs/REM_Offline_Engineering_Notes.pdf
  docs/REM_OFFLINE_INDEX.md
  docs/REM_CURRENT_TARGET_MANIFEST.md
  docs/REM_CURRENT_SYSCALL_REFERENCE.md
  docs/REM_ASSEMBLY_SYSCALL_COOKBOOK.md
  docs/REM_LINKER_SCRIPT_GUIDE.md
  docs/REM_BARE_METAL_IMAGE_RECIPE.md
  docs/REM_TERMINAL_CAPABILITY_MATRIX.md
  docs/REM_KNOWN_GOOD_FIXTURES.md
  docs/REM_OFFLINE_BUILD_MANIFEST.md
  docs/REM_C_COMPILER_STATUS.md
  docs/REM_VALIDATED_RUNTIME_TUPLES.md
  docs/REM_ARCHITECTURE64.md
  docs/MYEMULATOR_2_ABI.md
  docs/REM_SAMURAI_STATUS.md
  docs/REM_YASH_STATUS.md
  toolchain/userspace/myemulator2-user.ld
  toolchain/userspace/minilibc/crt0-linux.S
  toolchain/userspace/minilibc/include/stdio.h
  toolchain/chibicc-rem/README.md
  toolchain/chibicc-rem/PORT_STATUS.md
  toolchain/flight-kit-runtime/README.md
  toolchain/flight-kit-runtime/bin/chibicc
  toolchain/flight-kit-runtime/bin/as
  toolchain/flight-kit-runtime/bin/ld
  toolchain/flight-kit-runtime/bin/ar
  toolchain/flight-kit-runtime/bin/ranlib
  toolchain/flight-kit-runtime/bin/nm
  toolchain/flight-kit-runtime/bin/objdump
  toolchain/flight-kit-runtime/bin/readelf
  toolchain/flight-kit-runtime/bin/objcopy
  toolchain/flight-kit-runtime/bin/include/stddef.h
  toolchain/flight-kit-runtime/include/stdio.h
  toolchain/flight-kit-runtime/include/bits/alltypes.h
  toolchain/flight-kit-runtime/lib/crt1.o
  toolchain/flight-kit-runtime/lib/crti.o
  toolchain/flight-kit-runtime/lib/crtn.o
  toolchain/flight-kit-runtime/lib/libc.a
  toolchain/flight-kit-runtime/lib/libgcc.a
  toolchain/flight-kit-runtime/lib/myemulator2-user.ld
  toolchain/examples/flight-kit/README.md
  toolchain/examples/flight-kit/hello.c
  toolchain/examples/flight-kit/args.c
  toolchain/examples/flight-kit/filecopy.c
  toolchain/examples/flight-kit/malloc.c
  toolchain/examples/flight-kit/directory.c
  toolchain/examples/flight-kit/clock.c
  toolchain/examples/flight-kit/terminal.c
  toolchain/examples/flight-kit/signals.c
  toolchain/examples/flight-kit/process.c
  toolchain/examples/flight-kit/pipe.c
  toolchain/examples/flight-kit/asm-hello.S
  toolchain/examples/flight-kit/call-asm.c
  toolchain/examples/flight-kit/call-asm.S
  toolchain/examples/flight-kit/call-c.c
  toolchain/examples/flight-kit/call-c.S
  toolchain/examples/flight-kit/build.sh
  toolchain/examples/flight-kit/build.ninja.single
  toolchain/examples/flight-kit/build.ninja.multi
  toolchain/examples/flight-kit/build.ninja.mixed
  toolchain/scripts
  toolchain/examples
  linux/arch/myemulator2
  qemu/target/myemulator32
  tests/programs
)
optional=(
  .toolchain-install/bin/myemulator2-elf-as
  .toolchain-install/bin/myemulator2-elf-ld
  .qemu-build/qemu-system-myemulator32
  .linux-build/build/vmlinux
  toolchain/chibicc-rem/chibicc
)
bad=0
for item in "${required[@]}"; do
  if [[ -e "$ROOT/$item" ]]; then echo "PASS required $item"; else echo "FAIL required $item"; bad=1; fi
done
for item in "${optional[@]}"; do
  if [[ -e "$ROOT/$item" ]]; then echo "PASS optional $item"; else echo "WARN optional $item"; fi
done

# Report hosted-C components separately: presence of an incomplete build tree
# must not be mistaken for an installed guest sysroot.
hosted_c=(
  toolchain/flight-kit-runtime/include/stdio.h
  toolchain/flight-kit-runtime/include/bits/alltypes.h
  toolchain/flight-kit-runtime/lib/crt1.o
  toolchain/flight-kit-runtime/lib/crti.o
  toolchain/flight-kit-runtime/lib/crtn.o
  toolchain/flight-kit-runtime/lib/libc.a
  toolchain/flight-kit-runtime/lib/libgcc.a
  toolchain/flight-kit-runtime/lib/myemulator2-user.ld
  toolchain/flight-kit-runtime/bin/chibicc
  toolchain/flight-kit-runtime/bin/as
  toolchain/flight-kit-runtime/bin/ld
)
for item in "${hosted_c[@]}"; do
  if [[ -e "$ROOT/$item" ]]; then
    echo "PASS hosted-C component $item"
  else
    echo "FAIL hosted-C component $item"
    bad=1
  fi
done

declare -A expected_hashes=(
  [toolchain/flight-kit-runtime/bin/chibicc]=7b9134caaf776953d82d34c3f54dec0eb5b3f343d8ec92ff350df6ee82023a75
  [toolchain/flight-kit-runtime/bin/as]=c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93
  [toolchain/flight-kit-runtime/bin/ld]=edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65
  [toolchain/flight-kit-runtime/lib/crt1.o]=9951b664ec350b8ae34c30be9824a42b3a76253978515d674bf1d33b346a4697
  [toolchain/flight-kit-runtime/lib/crti.o]=7fb830f63e7ba115f433b6f28253e8510bbc7b0ab0708c62a2a0ac94aeba5f37
  [toolchain/flight-kit-runtime/lib/crtn.o]=5594cc2bd0fa13d22e159c78c1f3065b7f36ae217badcdeaf8e71b5c3ad64937
  [toolchain/flight-kit-runtime/lib/libc.a]=30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924
  [toolchain/flight-kit-runtime/lib/libgcc.a]=759fb011a7d4dfba4da8a96d17cc978485f2daf62c2ba4545180d4de40773044
  [toolchain/flight-kit-runtime/lib/myemulator2-user.ld]=6ec14dea3bda5ded2ef8b23ac1995d6b40857e2273ead8e095d57f25e8a54621
)
for item in "${!expected_hashes[@]}"; do
  if [[ ! -f "$ROOT/$item" ]]; then continue; fi
  actual=$(sha256sum "$ROOT/$item" | awk '{print $1}')
  if [[ "$actual" == "${expected_hashes[$item]}" ]]; then
    echo "PASS tuple hash $item"
  else
    echo "FAIL tuple hash $item expected ${expected_hashes[$item]} got $actual"
    bad=1
  fi
done
exit "$bad"
