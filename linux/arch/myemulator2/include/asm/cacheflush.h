/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_MYEMULATOR2_CACHEFLUSH_H
#define _ASM_MYEMULATOR2_CACHEFLUSH_H

/* Ask the TCG CPU implementation to discard translated instruction blocks.
 * REM has no hardware I-cache, but ptrace modifies guest code through kernel
 * page mappings that bypass QEMU's translated guest-store invalidation. */
static inline void myemulator2_flush_icache_range(unsigned long start,
						 unsigned long end)
{
	(void)start;
	(void)end;
	asm volatile (".word 0x25400000" ::: "memory");
}

#define flush_icache_range(start, end) \
	myemulator2_flush_icache_range(start, end)

#include <asm-generic/cacheflush.h>

#endif
