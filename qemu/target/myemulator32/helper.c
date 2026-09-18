#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/cputlb.h"
#include "exec/helper-proto.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
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

static G_NORETURN void myemu32_enter_exception(CPUMyEmulator32State *env,
                                     uint32_t cause, uint32_t saved_pc,
                                     uint32_t info, unsigned irq_level,
                                     bool nmi)
{
    CPUState *cs = env_cpu(env);
    uint32_t old_sr = env->sr;
    uint32_t old_ssp;
    uint32_t handler;
    MemTxResult result = MEMTX_OK;

    myemu32_save_visible_sp(env);
    old_ssp = env->ssp;
    env->sr |= MYEMU32_SR_S;
    myemu32_select_visible_sp(env);
    if (env->ssp < MYEMU32_FRAME_SIZE) {
        myemu32_halt_bad_vector(env, cause, 0);
        cpu_loop_exit(cs);
    }
    env->ssp -= MYEMU32_FRAME_SIZE;
    env->r[13] = env->ssp;

    address_space_stl_le(&address_space_memory, env->ssp + 0,
                         saved_pc, MEMTXATTRS_UNSPECIFIED, &result);
    address_space_stl_le(&address_space_memory, env->ssp + 4,
                         old_sr, MEMTXATTRS_UNSPECIFIED, &result);
    address_space_stl_le(&address_space_memory, env->ssp + 8,
                         cause, MEMTXATTRS_UNSPECIFIED, &result);
    address_space_stl_le(&address_space_memory, env->ssp + 12,
                         info, MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK) {
        myemu32_halt_bad_vector(env, cause, 0);
        cpu_loop_exit(cs);
    }

    if (irq_level) {
        env->sr = (env->sr & ~MYEMU32_SR_IPL_MASK) | (irq_level << 2);
    }
    handler = address_space_ldl_le(&address_space_memory,
                                   env->vbr + cause * 4,
                                   MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK || (handler & 3)) {
        myemu32_halt_bad_vector(env, cause, handler);
        cpu_loop_exit(cs);
    }
    env->pc = handler;
    env->halted = false;
    cs->halted = 0;
    cs->exception_index = cause;
    if (nmi) {
        env->nmi_active = true;
    }
    (void)old_ssp;
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
    uint32_t frame, saved_pc, saved_sr;
    MemTxResult result = MEMTX_OK;

    if (!myemu32_supervisor(env)) {
        helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
    }
    frame = env->ssp;
    saved_pc = address_space_ldl_le(&address_space_memory, frame,
                                    MEMTXATTRS_UNSPECIFIED, &result);
    saved_sr = address_space_ldl_le(&address_space_memory, frame + 4,
                                    MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK) {
        helper_exception(env, MYEMU32_VECTOR_DATA_ALIGN, pc, frame);
    }
    if (saved_pc & 3) {
        helper_exception(env, MYEMU32_VECTOR_INSN_ALIGN, saved_pc, saved_pc);
    }
    env->ssp = frame + MYEMU32_FRAME_SIZE;
    env->sr = saved_sr;
    myemu32_select_visible_sp(env);
    env->pc = saved_pc;
    if (address_space_ldl_le(&address_space_memory, frame + 8,
                             MEMTXATTRS_UNSPECIFIED, &result) ==
        MYEMU32_VECTOR_NMI) {
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
};

uint32_t helper_mfsr(CPUMyEmulator32State *env, uint32_t sysreg,
                     uint32_t pc)
{
    if (!myemu32_supervisor(env)) {
        helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
    }
    switch (sysreg) {
    case MYEMU32_SYS_SR: return env->sr;
    case MYEMU32_SYS_USP: return env->usp;
    case MYEMU32_SYS_SSP: return env->ssp;
    case MYEMU32_SYS_VBR: return env->vbr;
    case MYEMU32_SYS_PTBR: return env->ptbr;
    case MYEMU32_SYS_MMCR: return env->mmcr;
    default:
        helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, 0);
    }
}

void helper_mtsr(CPUMyEmulator32State *env, uint32_t sysreg,
                 uint32_t value, uint32_t pc)
{
    if (!myemu32_supervisor(env)) {
        helper_exception(env, MYEMU32_VECTOR_PRIVILEGE, pc, 0);
    }
    switch (sysreg) {
    case MYEMU32_SYS_SR:
        myemu32_save_visible_sp(env);
        env->sr = value & (MYEMU32_SR_CF | MYEMU32_SR_OF |
                           MYEMU32_SR_IPL_MASK | MYEMU32_SR_S);
        myemu32_select_visible_sp(env);
        return;
    case MYEMU32_SYS_USP: env->usp = value; break;
    case MYEMU32_SYS_SSP: env->ssp = value; if (myemu32_supervisor(env)) env->r[13] = value; break;
    case MYEMU32_SYS_VBR:
        if (value & 0x3ff) helper_exception(env, MYEMU32_VECTOR_DATA_ALIGN, pc, value);
        env->vbr = value; break;
    case MYEMU32_SYS_PTBR:
        if (value & 0xfff) helper_exception(env, MYEMU32_VECTOR_DATA_ALIGN, pc, value);
        env->ptbr = value;
        tlb_flush(env_cpu(env));
        break;
    case MYEMU32_SYS_MMCR:
        if (env->mmcr != (value & 1)) {
            tlb_flush(env_cpu(env));
        }
        env->mmcr = value & 1;
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
