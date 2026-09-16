#include "qemu/osdep.h"
#include "cpu.h"
#include "accel/tcg/cpu-loop.h"
#include "exec/cputlb.h"
#include "exec/helper-proto.h"
#include "exec/page-protection.h"
#include "exec/target_page.h"

hwaddr myemulator_cpu_get_phys_addr_debug(CPUState *cs, vaddr addr)
{
    return addr & 0xffff;
}

bool myemulator_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                             MMUAccessType access_type, int mmu_idx,
                             bool probe, uintptr_t retaddr)
{
    address &= TARGET_PAGE_MASK;
    tlb_set_page(cs, address, address, PAGE_READ | PAGE_WRITE | PAGE_EXEC,
                 mmu_idx, TARGET_PAGE_SIZE);
    return true;
}

void myemulator_cpu_do_interrupt(CPUState *cs)
{
}

bool myemulator_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    return false;
}

void helper_halt(CPUMyEmulatorState *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_HLT;
    cpu_loop_exit(cs);
}

void helper_illegal(CPUMyEmulatorState *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_MYEMU_ILLEGAL;
    cpu_loop_exit(cs);
}
