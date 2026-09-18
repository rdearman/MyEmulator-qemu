#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/exec-all.h"
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

void helper_exception(CPUMyEmulator32State *env, uint32_t cause,
                      uint32_t fault_pc, uint32_t info)
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
                         fault_pc, MEMTXATTRS_UNSPECIFIED, &result);
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
    (void)old_ssp;
    cpu_loop_exit(cs);
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
        if (value & 0x3ff) helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, value);
        env->vbr = value; break;
    case MYEMU32_SYS_PTBR:
        if (value & 0xfff) helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, value);
        env->ptbr = value; break;
    case MYEMU32_SYS_MMCR:
        /* Page walks are a later milestone.  Do not enable an unimplemented MMU. */
        if (value & 1) helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, value);
        env->mmcr = value & 1;
        break;
    default:
        helper_exception(env, MYEMU32_VECTOR_ILLEGAL, pc, 0);
    }
}
