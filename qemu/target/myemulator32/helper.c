#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/cputlb.h"
#include "exec/helper-proto.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "qemu/bswap.h"
#include "sysemu/runstate.h"

static bool myemu32_supervisor(CPUMyEmulator32State *env)
{
    return (env->sr & MYEMU32_SR_S) != 0;
}

static void myemu32_save_visible_sp(CPUMyEmulator32State *env)
{
    if (myemu32_supervisor(env)) {
        env->ssp = env->r[13];
    } else {
        env->usp = env->r[13];
    }
}

static void myemu32_select_visible_sp(CPUMyEmulator32State *env)
{
    env->r[13] = myemu32_supervisor(env) ? env->ssp : env->usp;
}

static void myemu32_halt_bad_vector(CPUMyEmulator32State *env,
                                    uint32_t vector, uint32_t target)
{
    CPUState *cs = env_cpu(env);

    qemu_log_mask(LOG_GUEST_ERROR,
                  "MyEmulator32: invalid exception vector %u target 0x%08x\n",
                  vector, target);
    env->halted = true;
    cs->halted = 1;
    cs->exception_index = MYEMU32_EXCP_ILLEGAL;
}

static bool myemu32_frame_access(CPUMyEmulator32State *env,
                                 uint32_t frame, bool write,
                                 uint32_t values[4])
{
    CPUState *cs = env_cpu(env);
    void *host[4];
    CPUTLBEntryFull *full;
    hwaddr len = MYEMU32_FRAME_SIZE;
    int flags;

    if (!env->mmcr) {
        void *base = address_space_map(&address_space_memory, frame, &len,
                                       write, MEMTXATTRS_UNSPECIFIED);
        if (!base || len < MYEMU32_FRAME_SIZE) {
            if (base) {
                address_space_unmap(&address_space_memory, base,
                                    len, write, len);
            }
            return false;
        }
        if (write) {
            for (unsigned i = 0; i < 4; i++) {
                stl_le_p((uint8_t *)base + i * 4, values[i]);
            }
        } else {
            for (unsigned i = 0; i < 4; i++) {
                values[i] = ldl_le_p((uint8_t *)base + i * 4);
            }
        }
        address_space_unmap(&address_space_memory, base, len, write,
                            MYEMU32_FRAME_SIZE);
        return true;
    }

    for (unsigned i = 0; i < 4; i++) {
        flags = probe_access_full(env, frame + i * 4, 4,
                                  write ? MMU_DATA_STORE : MMU_DATA_LOAD,
                                  MMU_SUPERVISOR_IDX, true, &host[i], &full, 0);
        if (flags & (TLB_INVALID_MASK | TLB_MMIO)) {
            return false;
        }
    }
    if (write) {
        for (unsigned i = 0; i < 4; i++) {
            stl_le_p(host[i], values[i]);
        }
    } else {
        for (unsigned i = 0; i < 4; i++) {
            values[i] = ldl_le_p(host[i]);
        }
    }
    (void)cs;
    return true;
}

static G_NORETURN void myemu32_enter_double_fault(CPUMyEmulator32State *env,
                                                   uint32_t saved_pc,
                                                   uint32_t saved_sr,
                                                   uint32_t original_cause)
{
    CPUState *cs = env_cpu(env);
    uint32_t values[4] = { saved_pc, saved_sr,
                           MYEMU32_VECTOR_DOUBLE_FAULT, original_cause };
    uint32_t frame;
    uint32_t handler;
    MemTxResult result = MEMTX_OK;

    if (env->dfsp < MYEMU32_FRAME_SIZE || (env->dfsp & 3)) {
        myemu32_halt_bad_vector(env, MYEMU32_VECTOR_DOUBLE_FAULT, 0);
        cpu_loop_exit(cs);
    }
    frame = env->dfsp - MYEMU32_FRAME_SIZE;
    env->sr = saved_sr | MYEMU32_SR_S;
    env->dfsp = frame;
    env->ssp = frame;
    env->r[13] = frame;
    {
        hwaddr len = MYEMU32_FRAME_SIZE;
        void *base = address_space_map(&address_space_memory, frame, &len,
                                       true, MEMTXATTRS_UNSPECIFIED);
        if (!base || len < MYEMU32_FRAME_SIZE) {
            if (base) {
                address_space_unmap(&address_space_memory, base, len, true, len);
            }
            myemu32_halt_bad_vector(env, MYEMU32_VECTOR_DOUBLE_FAULT, 0);
            cpu_loop_exit(cs);
        }
        for (unsigned i = 0; i < 4; i++) {
            stl_le_p((uint8_t *)base + i * 4, values[i]);
        }
        address_space_unmap(&address_space_memory, base, len, true,
                            MYEMU32_FRAME_SIZE);
    }
    handler = address_space_ldl_le(&address_space_memory,
                                   env->vbr + MYEMU32_VECTOR_DOUBLE_FAULT * 4,
                                   MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK || (handler & 3)) {
        myemu32_halt_bad_vector(env, MYEMU32_VECTOR_DOUBLE_FAULT, handler);
        cpu_loop_exit(cs);
    }
    env->pc = handler;
    env->halted = false;
    cs->halted = 0;
    cs->exception_index = MYEMU32_EXCP_DOUBLE_FAULT;
    cpu_loop_exit(cs);
}

static G_NORETURN void myemu32_enter_exception(CPUMyEmulator32State *env,
                                     uint32_t cause, uint32_t saved_pc,
                                     uint32_t info, unsigned irq_level,
                                     bool nmi)
{
    CPUState *cs = env_cpu(env);
    uint32_t old_sr = env->sr;
    uint32_t frame;
    uint32_t handler;
    MemTxResult result = MEMTX_OK;

    myemu32_save_visible_sp(env);
    env->sr |= MYEMU32_SR_S;
    myemu32_select_visible_sp(env);
    if ((env->ssp & 3) || (!env->mmcr && env->ssp < MYEMU32_FRAME_SIZE)) {
        myemu32_enter_double_fault(env, saved_pc, old_sr, cause);
    }
    frame = env->ssp - MYEMU32_FRAME_SIZE;
    {
        uint32_t values[4] = { saved_pc, old_sr, cause, info };
        if (!myemu32_frame_access(env, frame, true, values)) {
            myemu32_enter_double_fault(env, saved_pc, old_sr, cause);
        }
    }
    env->ssp = frame;
    env->r[13] = frame;

    if (irq_level) {
        env->sr = (env->sr & ~MYEMU32_SR_IPL_MASK) | (irq_level << 2);
    }
    handler = address_space_ldl_le(&address_space_memory,
                                   env->vbr + cause * 4,
                                   MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK || (handler & 3)) {
        if (cause == MYEMU32_VECTOR_DOUBLE_FAULT) {
            myemu32_halt_bad_vector(env, cause, handler);
            cpu_loop_exit(cs);
        }
        myemu32_enter_double_fault(env, saved_pc, old_sr, cause);
    }
    env->pc = handler;
    env->halted = false;
    cs->halted = 0;
    cs->exception_index = cause;
    if (nmi) {
        env->nmi_active = true;
    }
    cpu_loop_exit(cs);
}

void helper_exception(CPUMyEmulator32State *env, uint32_t cause,
                      uint32_t fault_pc, uint32_t info)
{
    myemu32_enter_exception(env, cause, fault_pc, info, 0, false);
}

void helper_irq(CPUMyEmulator32State *env, uint32_t level)
{
    myemu32_enter_exception(env, MYEMU32_VECTOR_IRQ1 + level - 1,
                            env->pc, level, level, false);
}

void helper_nmi(CPUMyEmulator32State *env)
{
    myemu32_enter_exception(env, MYEMU32_VECTOR_NMI, env->pc, 0, 0, true);
}

void helper_halt(CPUMyEmulator32State *env)
{
    CPUState *cs = env_cpu(env);

    if (!myemu32_supervisor(env)) {
        helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, env->pc, 0);
    }
    env->halted = true;
    cs->halted = 1;
    cs->exception_index = EXCP_HLT;
    if (runstate_is_running()) {
        vm_stop(RUN_STATE_DEBUG);
    }
    cpu_loop_exit(cs);
}

void helper_rfe(CPUMyEmulator32State *env, uint32_t pc)
{
    CPUState *cs = env_cpu(env);
    uint32_t frame, saved_pc, saved_sr, cause;
    uint32_t values[4];

    if (!myemu32_supervisor(env)) {
        helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
    }
    frame = env->ssp;
    if (!myemu32_frame_access(env, frame, false, values)) {
        helper_exception(env, MYEMU32_VECTOR_DATA_ALIGN, pc, frame);
    }
    saved_pc = values[0];
    saved_sr = values[1];
    cause = values[2];
    if (saved_pc & 3) {
        helper_exception(env, MYEMU32_VECTOR_INSN_ALIGN, saved_pc, saved_pc);
    }
    env->ssp = frame + MYEMU32_FRAME_SIZE;
    env->sr = saved_sr & (MYEMU32_SR_CF | MYEMU32_SR_OF |
                          MYEMU32_SR_IPL_MASK | MYEMU32_SR_S);
    myemu32_select_visible_sp(env);
    env->pc = saved_pc;
    if (cause == MYEMU32_VECTOR_NMI) {
        env->nmi_active = false;
    }
    env->halted = false;
    cs->halted = 0;
}

enum {
    MYEMU32_SYS_SR = 0,
    MYEMU32_SYS_USP = 1,
    MYEMU32_SYS_SSP = 2,
    MYEMU32_SYS_VBR = 3,
    MYEMU32_SYS_PTBR = 4,
    MYEMU32_SYS_MMCR = 5,
    MYEMU32_SYS_TIME_LO = 6,
    MYEMU32_SYS_TIME_HI = 7,
    MYEMU32_SYS_TIMECMP_LO = 8,
    MYEMU32_SYS_TIMECMP_HI = 9,
    MYEMU32_SYS_TP = 10,
    MYEMU32_SYS_DFSP = 11,
};

uint32_t helper_mfsr(CPUMyEmulator32State *env, uint32_t sysreg,
                     uint32_t pc)
{
    bool user_allowed = sysreg == MYEMU32_SYS_TIME_LO ||
                        sysreg == MYEMU32_SYS_TIME_HI ||
                        sysreg == MYEMU32_SYS_TP;
    if (!myemu32_supervisor(env) && !user_allowed) {
        helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
    }
    switch (sysreg) {
    case MYEMU32_SYS_SR: return env->sr;
    case MYEMU32_SYS_USP: return env->usp;
    case MYEMU32_SYS_SSP: return env->ssp;
    case MYEMU32_SYS_VBR: return env->vbr;
    case MYEMU32_SYS_PTBR: return env->ptbr;
    case MYEMU32_SYS_MMCR: return env->mmcr;
    case MYEMU32_SYS_TIME_LO:
    {
        uint64_t now = myemulator32_cpu_time_us(env);
        env->time_hi_latch = now >> 32;
        return (uint32_t)now;
    }
    case MYEMU32_SYS_TIME_HI: return env->time_hi_latch;
    case MYEMU32_SYS_TIMECMP_LO: return (uint32_t)env->timecmp;
    case MYEMU32_SYS_TIMECMP_HI: return env->timecmp >> 32;
    case MYEMU32_SYS_TP: return env->tp;
    case MYEMU32_SYS_DFSP: return env->dfsp;
    default:
        helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, 0);
    }
}

void helper_mtsr(CPUMyEmulator32State *env, uint32_t sysreg,
                 uint32_t value, uint32_t pc)
{
    switch (sysreg) {
    case MYEMU32_SYS_SR:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        myemu32_save_visible_sp(env);
        env->sr = value & (MYEMU32_SR_CF | MYEMU32_SR_OF |
                           MYEMU32_SR_IPL_MASK | MYEMU32_SR_S);
        myemu32_select_visible_sp(env);
        return;
    case MYEMU32_SYS_USP:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        env->usp = value;
        break;
    case MYEMU32_SYS_SSP:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        env->ssp = value;
        if (myemu32_supervisor(env)) {
            env->r[13] = value;
        }
        break;
    case MYEMU32_SYS_VBR:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        if (value & 0x3ff) helper_exception(env, MYEMU32_VECTOR_DATA_ALIGN, pc, value);
        env->vbr = value; break;
    case MYEMU32_SYS_PTBR:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        if (value & 0xfff) helper_exception(env, MYEMU32_VECTOR_DATA_ALIGN, pc, value);
        env->ptbr = value;
        tlb_flush(env_cpu(env));
        break;
    case MYEMU32_SYS_MMCR:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        if (env->mmcr != (value & 1)) {
            tlb_flush(env_cpu(env));
        }
        env->mmcr = value & 1;
        break;
    case MYEMU32_SYS_TIME_LO:
    case MYEMU32_SYS_TIME_HI:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, 0);
        break;
    case MYEMU32_SYS_TIMECMP_LO:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        env->timecmp_shadow = (env->timecmp_shadow & UINT64_C(0xffffffff00000000)) |
                              value;
        break;
    case MYEMU32_SYS_TIMECMP_HI:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        env->timecmp_shadow = (env->timecmp_shadow & UINT64_C(0xffffffff)) |
                              ((uint64_t)value << 32);
        env->timecmp = env->timecmp_shadow;
        env->timecmp_armed = true;
        if (env->time_irq_asserted || (env->irq_asserted & (1u << 1))) {
            env->time_irq_asserted = false;
            myemulator32_cpu_set_irq(env_cpu(env), 1, false);
        }
        myemulator32_cpu_program_timecmp(env);
        break;
    case MYEMU32_SYS_TP:
        env->tp = value;
        break;
    case MYEMU32_SYS_DFSP:
        if (!myemu32_supervisor(env)) {
            helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
        }
        env->dfsp = value;
        break;
    default:
        helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, 0);
    }
}

void helper_syscall(CPUMyEmulator32State *env, uint32_t immediate,
                    uint32_t pc)
{
    helper_exception(env, MYEMU32_VECTOR_SYSCALL, pc, immediate & 0x03ffffff);
}

void helper_breakpoint(CPUMyEmulator32State *env, uint32_t pc)
{
    if (!myemu32_supervisor(env)) {
        helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
    }
    helper_exception(env, MYEMU32_VECTOR_BREAKPOINT, pc, 0);
}

void helper_tlbflush(CPUMyEmulator32State *env, uint32_t page,
                     uint32_t ra, uint32_t pc)
{
    CPUState *cs = env_cpu(env);

    if (!myemu32_supervisor(env)) {
        helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
    }
    if (page == 0) {
        tlb_flush(cs);
    } else if (page == 1) {
        tlb_flush_page(cs, env->r[ra]);
    } else {
        helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, 0);
    }
}

static void myemu32_set_arith_flags(CPUMyEmulator32State *env,
                                    uint32_t lhs, uint32_t rhs,
                                    uint32_t result, bool borrow)
{
    bool overflow;

    if (borrow) {
        env->sr = (env->sr & ~(MYEMU32_SR_CF | MYEMU32_SR_OF)) |
                  ((lhs < rhs) ? MYEMU32_SR_CF : 0);
        overflow = (((lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0;
    } else {
        env->sr = (env->sr & ~(MYEMU32_SR_CF | MYEMU32_SR_OF)) |
                  ((result < lhs) ? MYEMU32_SR_CF : 0);
        overflow = ((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0;
    }
    if (overflow) {
        env->sr |= MYEMU32_SR_OF;
    }
}

uint32_t helper_adc(CPUMyEmulator32State *env, uint32_t lhs,
                    uint32_t rhs, uint32_t carry)
{
    uint64_t wide = (uint64_t)lhs + rhs + (carry & 1);
    uint32_t result = (uint32_t)wide;
    uint32_t rhs_effective = rhs + (carry & 1);

    myemu32_set_arith_flags(env, lhs, rhs_effective, result, false);
    if (rhs_effective < rhs) {
        env->sr |= MYEMU32_SR_CF;
    }
    return result;
}

uint32_t helper_sbc(CPUMyEmulator32State *env, uint32_t lhs,
                    uint32_t rhs, uint32_t borrow)
{
    uint64_t wide = (uint64_t)lhs - rhs - (borrow & 1);
    uint32_t result = (uint32_t)wide;
    uint32_t rhs_effective = rhs + (borrow & 1);

    myemu32_set_arith_flags(env, lhs, rhs_effective, result, true);
    if (rhs_effective < rhs) {
        env->sr |= MYEMU32_SR_CF;
    }
    return result;
}

static uint32_t myemu32_divide(CPUMyEmulator32State *env, uint32_t lhs,
                               uint32_t rhs, uint32_t pc, bool unsigned_op,
                               bool remainder)
{
    if (rhs == 0) {
        helper_exception(env, MYEMU32_VECTOR_DIV_ZERO, pc, 0);
    }
    if (!unsigned_op && lhs == 0x80000000u && rhs == 0xffffffffu) {
        helper_exception(env, MYEMU32_VECTOR_ARITH_OVERFLOW, pc, 0);
    }
    if (unsigned_op) {
        return remainder ? lhs % rhs : lhs / rhs;
    }
    {
        int64_t dividend = (int32_t)lhs;
        int64_t divisor = (int32_t)rhs;
        int64_t value = remainder ? dividend % divisor : dividend / divisor;
        return (uint32_t)(int32_t)value;
    }
}

uint32_t helper_div(CPUMyEmulator32State *env, uint32_t lhs,
                    uint32_t rhs, uint32_t pc)
{
    return myemu32_divide(env, lhs, rhs, pc, false, false);
}

uint32_t helper_divu(CPUMyEmulator32State *env, uint32_t lhs,
                     uint32_t rhs, uint32_t pc)
{
    return myemu32_divide(env, lhs, rhs, pc, true, false);
}

uint32_t helper_rem(CPUMyEmulator32State *env, uint32_t lhs,
                    uint32_t rhs, uint32_t pc)
{
    return myemu32_divide(env, lhs, rhs, pc, false, true);
}

uint32_t helper_remu(CPUMyEmulator32State *env, uint32_t lhs,
                     uint32_t rhs, uint32_t pc)
{
    return myemu32_divide(env, lhs, rhs, pc, true, true);
}
