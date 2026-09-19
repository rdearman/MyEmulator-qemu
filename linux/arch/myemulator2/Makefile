# SPDX-License-Identifier: GPL-2.0
KBUILD_DEFCONFIG := myemulator2_defconfig

KBUILD_CFLAGS += -D__linux__ -fno-pic

head-y := arch/$(ARCH)/kernel/head.o

core-y += arch/$(ARCH)/kernel/
core-y += arch/$(ARCH)/mm/
libs-y += arch/$(ARCH)/lib/

boot := arch/$(ARCH)/boot

PHONY += vmlinux.bin
vmlinux.bin: vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@
