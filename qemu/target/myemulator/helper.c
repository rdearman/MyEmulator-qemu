#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/cputlb.h"
#include "exec/helper-proto.h"
#include "exec/page-protection.h"
#include "exec/target_page.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "hw/myemulator/myemulator-debug.h"
#include "sysemu/runstate.h"

unsigned myemulator_cpu_highest_irq(CPUMyEmulatorState *env)
{
    unsigned ipl = (env->s0 & MYEMULATOR_S0_IPL_MASK) >> 4;

    for (unsigned level = MYEMULATOR_IRQ_COUNT; level > ipl; level--) {
        if (env->irq_asserted & (1U << level)) {
            return level;
        }
    }
    return 0;
}

void myemulator_cpu_set_irq(CPUState *cs, unsigned level, bool asserted)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    if (level == 0 || level > MYEMULATOR_IRQ_COUNT) {
        return;
    }
    if (asserted) {
        env->irq_asserted |= 1U << level;
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        env->irq_asserted &= ~(1U << level);
        if (env->irq_asserted == 0) {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
        }
    }
}

void myemulator_cpu_set_irq_mask(CPUState *cs, unsigned mask)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    env->irq_asserted = mask & 0xfe;
    if (env->irq_asserted) {
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
    }
}

void myemulator_cpu_set_irq_oneshot(CPUState *cs, bool oneshot)
{
    cpu_env(cs)->irq_oneshot = oneshot;
}

void myemulator_cpu_set_irq_after_entry(CPUState *cs, unsigned level)
{
    cpu_env(cs)->irq_after_entry = level <= MYEMULATOR_IRQ_COUNT ? level : 0;
}

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

static bool myemulator_vector_target(CPUMyEmulatorState *env, hwaddr vector,
                                     uint16_t *target)
{
    CPUState *cs = env_cpu(env);
    uint16_t value = address_space_lduw_le(&address_space_memory, vector,
                                           MEMTXATTRS_UNSPECIFIED, NULL);

    if (value & 1) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "MyEmulator: odd exception/IRQ vector target 0x%04x\n",
                      value);
        env->pc = value;
        env->halted = true;
        cs->halted = 1;
        return false;
    }
    *target = value;
    return true;
}

static void myemulator_push_frame(CPUMyEmulatorState *env, uint32_t pc,
                                   uint8_t s0)
{
    env->sp = (env->sp - 1) & 0xffff;
    address_space_stb(&address_space_memory, env->sp, pc & 0xff,
                      MEMTXATTRS_UNSPECIFIED, NULL);
    env->sp = (env->sp - 1) & 0xffff;
    address_space_stb(&address_space_memory, env->sp, pc >> 8,
                      MEMTXATTRS_UNSPECIFIED, NULL);
    env->sp = (env->sp - 1) & 0xffff;
    address_space_stb(&address_space_memory, env->sp, s0,
                      MEMTXATTRS_UNSPECIFIED, NULL);
}

static void myemulator_enter_vector(CPUMyEmulatorState *env, hwaddr vector)
{
    uint16_t target;

    if (myemulator_vector_target(env, vector, &target)) {
        env->pc = target;
        env->halted = false;
        env_cpu(env)->halted = 0;
    }
}

void myemulator_cpu_do_interrupt(CPUState *cs)
{
    CPUMyEmulatorState *env = cpu_env(cs);
    unsigned level = myemulator_cpu_highest_irq(env);
    uint32_t pc = env->pc & 0xffff;
    uint8_t s0 = env->s0 & 0xff;

    if (level == 0) {
        return;
    }

    /* Match the descending PUSH convention: low PC, high PC, then S0. */
    myemulator_push_frame(env, pc, s0);

    env->s0 = (s0 & ~MYEMULATOR_S0_IPL_MASK) | (level << 4);
    if (env->irq_oneshot) {
        env->irq_asserted &= ~(1U << level);
        if (env->irq_asserted == 0) {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
        }
    }
    if (env->irq_after_entry != 0) {
        unsigned next = env->irq_after_entry;

        env->irq_after_entry = 0;
        myemulator_cpu_set_irq(cs, next, true);
    }
    myemulator_enter_vector(env, MYEMULATOR_IRQ_VECTOR(level));
}

bool myemulator_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    if ((interrupt_request & CPU_INTERRUPT_HARD) == 0 ||
        myemulator_cpu_highest_irq(cpu_env(cs)) == 0) {
        return false;
    }
    myemulator_cpu_do_interrupt(cs);
    return true;
}

bool myemulator_cpu_debug_check_breakpoint(CPUState *cs)
{
    /* MyEmulator has no architectural debug modes; every QEMU CPU
     * breakpoint is an execution breakpoint. */
    return true;
}

void helper_halt(CPUMyEmulatorState *env)
{
    CPUState *cs = env_cpu(env);

    env->halted = true;
    cs->halted = 1;
    cs->exception_index = EXCP_HLT;
    if (myemulator_debug_is_running() || cs->singlestep_enabled) {
        /* vm_stop() is safe from the vCPU thread and emits the QMP STOP
         * transition even when HALT was reached during single-step. */
        vm_stop(RUN_STATE_DEBUG);
    }
    cpu_loop_exit(cs);
}

void helper_illegal(CPUMyEmulatorState *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_MYEMU_ILLEGAL;
    cpu_loop_exit(cs);
}

void helper_alignment_exception(CPUMyEmulatorState *env, uint32_t fault_pc)
{
    CPUState *cs = env_cpu(env);

    /* Alignment faults are synchronous and do not alter live S0/IPL. */
    env->pc = fault_pc & 0xffff;
    myemulator_push_frame(env, env->pc, env->s0 & 0xff);
    cs->exception_index = EXCP_MYEMU_ALIGNMENT;
    myemulator_enter_vector(env, MYEMULATOR_ALIGNMENT_VECTOR);
    cpu_loop_exit(cs);
}
