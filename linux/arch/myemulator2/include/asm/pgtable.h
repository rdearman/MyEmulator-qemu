#ifndef _ASM_MYEMULATOR2_PGTABLE_H
#define _ASM_MYEMULATOR2_PGTABLE_H

#include <asm/page.h>
#include <linux/types.h>

/* Linux presents the hardware's 10/10/12 tables as a two-level pgd/pte
 * hierarchy and folds p4d/pud/pmd. */
typedef u32 pteval_t;
typedef u32 pgdval_t;
typedef struct { pteval_t pte; } pte_t;
typedef struct { pgdval_t pgd; } pgd_t;
typedef u32 pgprot_t;
struct page;
typedef struct page *pgtable_t;

#define pte_val(x) ((x).pte)
#define pgd_val(x) ((x).pgd)
#define pgprot_val(x) (x)
#define __pte(x) ((pte_t){ .pte = (x) })
#define __pgd(x) ((pgd_t){ .pgd = (x) })
#define __pgprot(x) ((pgprot_t)(x))

#define PTRS_PER_PGD 1024
#define PTRS_PER_PTE 1024
#define PTRS_PER_PMD 1
#define PTRS_PER_PUD 1
#define PGDIR_SHIFT 22
#define PGDIR_SIZE (1UL << PGDIR_SHIFT)
#define PGDIR_MASK (~(PGDIR_SIZE - 1))
#define PMD_SHIFT PGDIR_SHIFT
#define PUD_SHIFT PGDIR_SHIFT
#define P4D_SHIFT PGDIR_SHIFT
#define PTE_SHIFT PAGE_SHIFT
#define USER_PTRS_PER_PGD (TASK_SIZE >> PGDIR_SHIFT)
#define PFN_PTE_SHIFT PAGE_SHIFT
#define __PAGETABLE_PMD_FOLDED 1
#define __PAGETABLE_PUD_FOLDED 1
#define __PAGETABLE_P4D_FOLDED 1

#include <asm-generic/pgtable-nopmd.h>
#include <asm-generic/pgtable-nopud.h>

#define PAGE_NONE __pgprot(0)
#define PAGE_SHARED __pgprot(0x0f)
#define PAGE_COPY __pgprot(0x0d)
#define PAGE_READONLY __pgprot(0x15)
#define PAGE_KERNEL __pgprot(0x1f)
#define PAGE_KERNEL_EXEC __pgprot(0x1f)

#define _PAGE_PRESENT 0x001
#define _PAGE_USER    0x002
#define _PAGE_READ    0x004
#define _PAGE_WRITE   0x008
#define _PAGE_EXEC    0x010
#define _PAGE_ACCESSED 0x020
#define _PAGE_DIRTY   0x040

#define pte_none(pte) (pte_val(pte) == 0)
#define pte_present(pte) (pte_val(pte) & _PAGE_PRESENT)
#define pte_clear(mm, addr, ptep) (*(ptep) = __pte(0))
#define set_pte(ptep, pte) (*(ptep) = (pte))
#define pte_read(pte) (pte_val(pte) & _PAGE_READ)
#define pte_write(pte) (pte_val(pte) & _PAGE_WRITE)
#define pte_user(pte) (pte_val(pte) & _PAGE_USER)
#define pte_exec(pte) (pte_val(pte) & _PAGE_EXEC)
#define pte_dirty(pte) (pte_val(pte) & _PAGE_DIRTY)
#define pte_young(pte) (pte_val(pte) & _PAGE_ACCESSED)
#define pte_wrprotect(pte) __pte(pte_val(pte) & ~_PAGE_WRITE)
#define pte_mkwrite(pte, ...) __pte(pte_val(pte) | _PAGE_WRITE)
#define pte_mkdirty(pte) __pte(pte_val(pte) | _PAGE_DIRTY)
#define pte_mkclean(pte) __pte(pte_val(pte) & ~_PAGE_DIRTY)
#define pte_mkyoung(pte) __pte(pte_val(pte) | _PAGE_ACCESSED)
#define pte_mkold(pte) __pte(pte_val(pte) & ~_PAGE_ACCESSED)
#define pte_mkread(pte) __pte(pte_val(pte) | _PAGE_READ)

#define pmd_none(pmd) (pmd_val(pmd) == 0)
#define pmd_present(pmd) (pmd_val(pmd) & _PAGE_PRESENT)
#define pmd_bad(pmd) 0
#define pmd_clear(pmd) do { *(pmd) = __pmd(0); } while (0)

#ifndef __ASSEMBLY__
struct mm_struct;
extern pgd_t *pgd_alloc(struct mm_struct *mm);
static inline unsigned long pmd_page_vaddr(pmd_t pmd)
{ return pmd_val(pmd); }
static inline unsigned long pud_page_vaddr(pud_t pud)
{ return pud_val(pud); }
static inline unsigned long p4d_page_vaddr(p4d_t p4d)
{ return p4d_val(p4d); }
static inline struct page *pfn_to_page(unsigned long pfn)
{ return (struct page *)(pfn << PAGE_SHIFT); }
static inline unsigned long page_to_pfn(struct page *page)
{ return (unsigned long)page >> PAGE_SHIFT; }
static inline struct page *virt_to_page(const void *addr)
{ return pfn_to_page(virt_to_pfn(addr)); }
static inline unsigned long page_to_phys(struct page *page)
{ return page_to_pfn(page) << PAGE_SHIFT; }
static inline pte_t *pte_offset_kernel(pmd_t *pmd, unsigned long addr)
{ return (pte_t *)pmd_page_vaddr(*pmd) + ((addr >> PAGE_SHIFT) & 1023); }
#define pte_offset_kernel pte_offset_kernel
#define pmd_page(pmd) pfn_to_page(pmd_val(pmd) >> PAGE_SHIFT)
#define pte_page(pte) pfn_to_page(pte_val(pte) >> PAGE_SHIFT)
#define pte_pfn(pte) (pte_val(pte) >> PAGE_SHIFT)
#define ZERO_PAGE(vaddr) pfn_to_page(0)

/* Linux's generic swap code still needs a reversible software PTE form even
 * though the initial MyEmulator2 configuration has no swap device. */
#define __swp_type(x) (((x).val >> 26) & 0x1f)
#define __swp_offset(x) ((x).val & 0x03ffffff)
#define __swp_entry(type, off) ((swp_entry_t){ .val = (((type) & 0x1f) << 26) | ((off) & 0x03ffffff) })
#define __pte_to_swp_entry(pte) ((swp_entry_t){ .val = pte_val(pte) })
#define __swp_entry_to_pte(x) __pte((x).val)
#define _PAGE_SWP_EXCLUSIVE 0x080
static inline int pte_swp_exclusive(pte_t pte)
{ return pte_val(pte) & _PAGE_SWP_EXCLUSIVE; }
static inline pte_t pte_swp_mkexclusive(pte_t pte)
{ return __pte(pte_val(pte) | _PAGE_SWP_EXCLUSIVE); }
static inline pte_t pte_swp_clear_exclusive(pte_t pte)
{ return __pte(pte_val(pte) & ~_PAGE_SWP_EXCLUSIVE); }
#endif

#endif
