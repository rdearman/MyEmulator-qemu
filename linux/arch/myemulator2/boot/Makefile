# SPDX-License-Identifier: GPL-2.0
targets := Image vmlinux.bin
$(obj)/Image: vmlinux FORCE
	$(call if_changed,objcopy)

$(obj)/vmlinux.bin: vmlinux FORCE
	$(call if_changed,objcopy)

OBJCOPYFLAGS_Image := -O binary -R .note -R .comment -S
OBJCOPYFLAGS_vmlinux.bin := -O binary -R .note -R .comment -S
