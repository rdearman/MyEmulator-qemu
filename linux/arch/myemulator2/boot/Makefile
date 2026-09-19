# SPDX-License-Identifier: GPL-2.0
$(obj)/Image: vmlinux FORCE
	$(call if_changed,objcopy)

OBJCOPYFLAGS_Image := -O binary -R .note -R .comment -S
obj-y += Image
