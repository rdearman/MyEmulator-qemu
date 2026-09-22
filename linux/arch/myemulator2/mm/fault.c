// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/perf_event.h>

#include <asm/ptrace.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>

/*
 * The initial port gives each address space its own top-level page table,
 * while the hardware requires the supervisor half of the address space to
 * be present in the active PTBR.  A vmalloc mapping may be created after a
 * user mm was allocated, so its kernel PGD entry is not necessarily in that
 * mm yet.  Synchronise the missing top-level entry on the architected
 * supervisor fault and retry the instruction.
 */
static bool myemulator2_sync_vmalloc_pgd(struct mm_struct *mm,
		unsigned long address)
{
	pgd_t *master;
	pgd_t *active;

	if (address < VMALLOC_START || address >= VMALLOC_END || !mm)
		return false;

	master = pgd_offset_k(address);
	active = pgd_offset(mm, address);
	if (pgd_val(*master) == 0 || pgd_val(*active) == pgd_val(*master))
		return false;

	*active = *master;
	flush_tlb_kernel_page(address);
	return true;
}

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long cause,
		unsigned long address)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	vm_fault_t fault;
	bool zero_new_anon = false;
	unsigned int flags = FAULT_FLAG_DEFAULT;
	bool user = !(regs->sr & (1u << 5));
	bool write = cause == 8 || cause == 9;
	bool exec = cause == 4 || cause == 5;
	if (user)
		flags |= FAULT_FLAG_USER;
	if (write)
		flags |= FAULT_FLAG_WRITE;
	if (unlikely(!mm)) {
		pr_emerg("REM page fault without mm: pid=%d pc=%08lx "
			 "sp=%08lx lr=%08lx sr=%08lx cause=%lu info=%08lx "
			 "r1=%08lx r2=%08lx r3=%08lx r4=%08lx r5=%08lx r6=%08lx\n",
			 current->pid, regs->pc, regs->r[13], regs->r[14],
			 regs->sr, cause, address, regs->r[1], regs->r[2], regs->r[3],
			 regs->r[4], regs->r[5], regs->r[6]);
		panic("REM page fault without mm");
	}

	retry_fault:
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
	/* The low kernel identity map overlaps the initial user image window.
	 * A copied identity PTE is valid for supervisor accesses but must be
	 * replaced before Linux installs the user mapping for an executable or
	 * writable VMA. */
	if (user) {
		pmd_t *pmd = pmd_off(mm, address);
		if (!pmd_none(*pmd)) {
			pte_t *ptep = pte_offset_kernel(pmd, address);
			if (pte_present(*ptep) && !pte_user(*ptep)) {
				pte_clear(mm, address, ptep);
				flush_tlb_page(vma, address);
			}
		}
	}
	/* The generic anonymous-page path normally guarantees a zeroed page.
	 * Keep that architectural invariant explicit here as well: this port
	 * can enter the fault handler for supervisor copy_{to,from}_user()
	 * accesses during ELF loading, and those accesses must not expose a
	 * recycled physical page as the new image's .bss. */
	if (!vma->vm_file) {
		pmd_t *pmd = pmd_off(mm, address);
		if (!pmd_none(*pmd))
			zero_new_anon = pte_none(*pte_offset_kernel(pmd, address));
	}

	fault = handle_mm_fault(vma, address, flags, regs);
	/* The generic folded-page-table path can preserve the copied kernel
	 * identity entry's supervisor-only flags while replacing its PTE.  A
	 * user VMA requires USER permission at both levels of the REM walk. */
	if (user) {
		pmd_t *pmd = pmd_off(mm, address);
		if (!pmd_none(*pmd) && !(pmd_val(*pmd) & _PAGE_USER))
			*pmd = __pmd(pmd_val(*pmd) | _PAGE_USER);
	}
	/* Generic fault handling may install a previously non-present
	 * executable PTE without going through the architecture's set_pte()
	 * helper.  Drop the failed instruction-fetch translation before RFE
	 * retries the faulting instruction. */
	if (exec && !(fault & VM_FAULT_ERROR))
		flush_tlb_page(vma, address);
	/* File-backed faults may drop mmap_lock while waiting for the block
	 * layer.  VM_FAULT_RETRY asks the architecture to reacquire the lock and
	 * retry the fault; unlocking unconditionally here corrupts the rwsem and
	 * leaves execve() stuck when an ELF is loaded from ext4. */
	if (fault & VM_FAULT_RETRY) {
		if (flags & FAULT_FLAG_ALLOW_RETRY) {
			flags &= ~FAULT_FLAG_ALLOW_RETRY;
			goto retry_fault;
		}
	}
	if (!(fault & VM_FAULT_ERROR) && zero_new_anon) {
		pmd_t *pmd = pmd_off(mm, address);
		pte_t *ptep = pte_offset_kernel(pmd, address);
		if (pte_present(*ptep)) {
			struct page *page = pfn_to_page(pte_pfn(*ptep));
			clear_user_page(page_address(page), address, page);
		}
	}
	mmap_read_unlock(mm);
	if (fault & VM_FAULT_ERROR)
		goto bad_area;
	return;

bad_area_unlock:
	mmap_read_unlock(mm);
bad_area:
	if (!user) {
		if (myemulator2_sync_vmalloc_pgd(mm, address))
			return;
		pr_emerg("REM kernel fault: pc=%08lx sp=%08lx lr=%08lx "
			 "sr=%08lx cause=%lu info=%08lx r1=%08lx r2=%08lx "
			 "r3=%08lx r4=%08lx r5=%08lx r6=%08lx\\n", regs->pc,
			 regs->r[13], regs->r[14], regs->sr, cause, address,
			 regs->r[1], regs->r[2], regs->r[3], regs->r[4], regs->r[5],
			 regs->r[6]);
		panic("REM kernel page fault");
	}
	force_sig_fault(SIGSEGV,
			(cause == 4 || cause == 6 || cause == 8) ?
			SEGV_MAPERR : SEGV_ACCERR,
			(void __user *)address);
}
