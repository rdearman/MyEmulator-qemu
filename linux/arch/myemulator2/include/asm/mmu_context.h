#ifndef _ASM_MYEMULATOR2_MMU_CONTEXT_H
#define _ASM_MYEMULATOR2_MMU_CONTEXT_H
#include <asm/mmu.h>
#include <asm/pgtable.h>
extern bool myemulator2_paging_ready;
static inline int init_new_context(struct task_struct *tsk, struct mm_struct *mm) { mm->context.pgd = 0; return 0; }
static inline void destroy_context(struct mm_struct *mm) { }
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next, struct task_struct *tsk)
{
	if (next && next->pgd) {
		unsigned long ptbr = (unsigned long)next->pgd;
		unsigned long mmcr = 1;
		asm volatile("mtsr ptbr, %0\n\tmtsr mmcr, %1" ::
				"r"(ptbr), "r"(mmcr) : "memory");
	}
}
static inline void activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	switch_mm(prev, next, NULL);
}
static inline void deactivate_mm(struct task_struct *tsk, struct mm_struct *mm)
{ (void)tsk; (void)mm; }
static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
	(void)mm;
	(void)tsk;
	/* A kernel thread, and a task after exit_mm(), has no user address
	 * space.  Keep supervisor execution on the master kernel tables rather
	 * than leaving the exiting task's PTBR active. */
	if (myemulator2_paging_ready)
		asm volatile("mtsr ptbr, %0\n\t tlbflush" ::
			"r"((unsigned long)swapper_pg_dir) : "memory");
}
static inline int arch_dup_mmap(struct mm_struct *oldmm, struct mm_struct *mm)
{ (void)oldmm; (void)mm; return 0; }
static inline void arch_exit_mmap(struct mm_struct *mm) { (void)mm; }
#endif
