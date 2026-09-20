/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_MYEMULATOR2_TLBFLUSH_H
#define _ASM_MYEMULATOR2_TLBFLUSH_H

struct mm_struct;
struct vm_area_struct;

static inline void flush_tlb_all(void)
{
	asm volatile("tlbflush" ::: "memory");
}
static inline void flush_tlb_mm(struct mm_struct *mm) { (void)mm; }
static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{ (void)vma; (void)start; (void)end; }
static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long address)
{ (void)vma; (void)address; }
static inline void flush_tlb_kernel_range(unsigned long start,
					  unsigned long end)
{
	(void)start;
	(void)end;
	flush_tlb_all();
}
static inline void flush_tlb_kernel_page(unsigned long address)
{
	(void)address;
	flush_tlb_all();
}

#endif
