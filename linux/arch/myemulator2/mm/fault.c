// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/perf_event.h>

#include <asm/ptrace.h>

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long cause,
		unsigned long address)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	vm_fault_t fault;
	unsigned int flags = FAULT_FLAG_DEFAULT;
	bool user = !(regs->sr & (1u << 5));
	bool write = cause == 8 || cause == 9;
	bool exec = cause == 4 || cause == 5;
	if (user)
		flags |= FAULT_FLAG_USER;
	if (write)
		flags |= FAULT_FLAG_WRITE;
	if (unlikely(!mm))
		panic("MyEmulator2 page fault without mm");

	perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS, 1, regs, address);
	mmap_read_lock(mm);
	vma = find_vma(mm, address);
	if (!vma || vma->vm_start > address)
		goto bad_area_unlock;
	if (write && !(vma->vm_flags & VM_WRITE))
		goto bad_area_unlock;
	if (exec && !(vma->vm_flags & VM_EXEC))
		goto bad_area_unlock;
	if (!write && !exec && !(vma->vm_flags & (VM_READ | VM_WRITE)))
		goto bad_area_unlock;

	fault = handle_mm_fault(vma, address, flags, regs);
	mmap_read_unlock(mm);
	if (fault & VM_FAULT_ERROR)
		goto bad_area;
	return;

bad_area_unlock:
	mmap_read_unlock(mm);
bad_area:
	if (!user)
		panic("MyEmulator2 kernel page fault");
	force_sig_fault(SIGSEGV,
			(cause == 4 || cause == 6 || cause == 8) ?
			SEGV_MAPERR : SEGV_ACCERR,
			(void __user *)address);
}
