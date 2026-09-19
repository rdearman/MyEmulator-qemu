/* SPDX-License-Identifier: GPL-2.0 */
#include <asm-generic/vmlinux.lds.h>

OUTPUT_ARCH(myemulator2)
ENTRY(_start)

SECTIONS
{
	. = 0x00100000;
	__vectors_start = .;
	.vectors : ALIGN(1024) {
		KEEP(*(.vectors))
	}
	.text : ALIGN(4) {
		_stext = .;
		*(.text.head)
		*(.text .text.*)
		_etext = .;
	}
	.rodata : ALIGN(16) {
		_rodata = .;
		*(.rodata .rodata.*)
		_rodata_end = .;
	}
	.data : ALIGN(16) {
		_sdata = .;
		*(.data .data.*)
		_edata = .;
	}
	.bss : ALIGN(16) {
		__bss_start = .;
		*(.bss .bss.* COMMON)
		__bss_end = .;
	}
	_end = .;
	/DISCARD/ : { *(.comment .note .eh_frame) }
}
