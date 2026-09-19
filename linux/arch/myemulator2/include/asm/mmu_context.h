#ifndef _ASM_MYEMULATOR2_MMU_CONTEXT_H
#define _ASM_MYEMULATOR2_MMU_CONTEXT_H
#include <asm/mmu.h>
static inline int init_new_context(struct task_struct *tsk, struct mm_struct *mm) { mm->context.pgd = 0; return 0; }
static inline void destroy_context(struct mm_struct *mm) { }
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next, struct task_struct *tsk) { }
static inline void activate_mm(struct mm_struct *prev, struct mm_struct *next) { }
static inline void deactivate_mm(struct task_struct *tsk, struct mm_struct *mm)
{ (void)tsk; (void)mm; }
static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{ (void)mm; (void)tsk; }
static inline int arch_dup_mmap(struct mm_struct *oldmm, struct mm_struct *mm)
{ (void)oldmm; (void)mm; return 0; }
static inline void arch_exit_mmap(struct mm_struct *mm) { (void)mm; }
#endif
