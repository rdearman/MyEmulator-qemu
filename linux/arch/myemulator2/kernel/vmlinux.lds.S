/* SPDX-License-Identifier: GPL-2.0 */
#include <asm-generic/vmlinux.lds.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#undef PAGE_SIZE
#define PAGE_SIZE 4096

#define LOAD_OFFSET 0

OUTPUT_ARCH(myemulator2)
ENTRY(_start)

SECTIONS
{
	. = 0x00100000;
	__vectors_start = .;
	.vectors : ALIGN(1024) {
		KEEP(*(.vectors))
	}
	_text = .;
	HEAD_TEXT_SECTION
	.text : ALIGN(4) {
		_stext = .;
		TEXT_TEXT
		SCHED_TEXT
		LOCK_TEXT
		IRQENTRY_TEXT
		SOFTIRQENTRY_TEXT
		_etext = .;
	}
	. = ALIGN(4096);
	__init_begin = .;
	INIT_TEXT_SECTION(4096)
	INIT_DATA_SECTION(16)
	__init_end = .;
	_sdata = .;
	RO_DATA(16)
	RW_DATA(16, 4096, 16)
	_edata = .;
	BSS_SECTION(16, 16, 16)
	EXCEPTION_TABLE(4)
	_end = .;
	__bss_end = __bss_stop;
	_text = _stext;
	jiffies = jiffies_64;
	/DISCARD/ : { *(.comment .note .eh_frame) }
}
