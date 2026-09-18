#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/address-spaces.h"
#include "exec/cpu-all.h"
#include "exec/exec-all.h"
#include "exec/cputlb.h"
#include "exec/translation-block.h"
#include "hw/core/tcg-cpu-ops.h"
#include "hw/core/sysemu-cpu-ops.h"
#include "qemu/qemu-print.h"

static void myemulator32_set_pc(CPUState *cs, vaddr value)
{
    CPUMyEmulator32State *env = cpu_env(cs);

    env->pc = value;
    if (value & 3) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "MyEmulator32: unaligned PC 0x%08" PRIx32 "\n",
                      env->pc);
        env->halted = true;
        cs->halted = 1;
    }
}

static vaddr myemulator32_get_pc(CPUState *cs)
{
    return cpu_env(cs)->pc;
}

static void myemulator32_sync_from_tb(CPUState *cs,
                                      const TranslationBlock *tb)
{
    cpu_env(cs)->pc = tb->pc;
}

static void myemulator32_restore_state(CPUState *cs,
                                       const TranslationBlock *tb,
                                       const uint64_t *data)
{
    cpu_env(cs)->pc = data[0];
}

static int myemulator32_mmu_index(CPUState *cs, bool ifetch)
{
    return MMU_PHYS_IDX;
}

static bool myemulator32_has_work(CPUState *cs)
{
    return !cpu_env(cs)->halted;
}

static bool myemulator32_exec_interrupt(CPUState *cs, int interrupt_request)
{
    if (interrupt_request & CPU_INTERRUPT_EXITTB) {
        cpu_reset_interrupt(cs, CPU_INTERRUPT_EXITTB);
        return true;
    }
    /* External IRQ delivery is intentionally deferred to the interrupt
     * milestone; this target has no interrupt sources yet. */
    return false;
}

static void myemulator32_do_interrupt(CPUState *cs)
{
    /* Synchronous exceptions perform their architectural entry in the
     * helper, before leaving the translation block.  QEMU still requires a
     * target callback so it can complete the exception-return path. */
    if (cs->exception_index == EXCP_HLT) {
        return;
    }
}

static const struct SysemuCPUOps myemulator32_sysemu_ops = {
    /* Debug physical-page lookup and external interrupt delivery are added
     * by later 2.0 milestones.  The non-NULL ops table is nevertheless
     * required by QEMU's common CPU realization path. */
};

bool myemulator32_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                               MMUAccessType access_type, int mmu_idx,
                               bool probe, uintptr_t retaddr)
{
    /* This milestone only executes with MMCR.EN clear.  The identity map
     * keeps the normal QEMU physical-memory path usable until page walks are
     * implemented by the MMU milestone. */
    tlb_set_page(cs, address & TARGET_PAGE_MASK, address & TARGET_PAGE_MASK,
                 PAGE_READ | PAGE_WRITE | PAGE_EXEC, mmu_idx,
                 TARGET_PAGE_SIZE);
    return true;
}

static void myemulator32_reset_hold(Object *obj, ResetType type)
{
    CPUState *cs = CPU(obj);
    MyEmulator32CPU *cpu = MYEMULATOR32_CPU(cs);
    MyEmulator32CPUClass *mcc = MYEMULATOR32_CPU_GET_CLASS(obj);
    CPUMyEmulator32State *env = &cpu->env;
    MemTxResult result = MEMTX_OK;

    if (mcc->parent_phases.hold) {
        mcc->parent_phases.hold(obj, type);
    }
    memset(env, 0, sizeof(*env));
    env->sr = MYEMU32_SR_S | (7u << 2);
    env->ssp = address_space_ldl_le(&address_space_memory, 0x400,
                                    MEMTXATTRS_UNSPECIFIED, &result);
    env->pc = address_space_ldl_le(&address_space_memory, 0x404,
                                   MEMTXATTRS_UNSPECIFIED, &result);
    env->r[13] = env->ssp;
    if (result != MEMTX_OK || (env->ssp & 3) || (env->pc & 3)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "MyEmulator32: invalid reset vectors SSP=0x%08" PRIx32
                      " PC=0x%08" PRIx32 "\n", env->ssp, env->pc);
        env->halted = true;
        cs->halted = 1;
    }
    cs->exception_index = -1;
}

static void myemulator32_realize(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    MyEmulator32CPUClass *mcc = MYEMULATOR32_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }
    qemu_init_vcpu(cs);
    mcc->parent_realize(dev, errp);
}

static ObjectClass *myemulator32_class_by_name(const char *cpu_model)
{
    g_autofree char *name = g_strdup_printf(MYEMULATOR32_CPU_TYPE_NAME("%s"),
                                            cpu_model);
    return object_class_by_name(name);
}

void myemulator32_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    CPUMyEmulator32State *env = cpu_env(cs);

    for (int i = 0; i < 16; i++) {
        qemu_fprintf(f, "R%d: 0x%08" PRIx32 "%s", i,
                     i == 0 ? 0 : env->r[i], i == 7 ? "\n" : "  ");
    }
    qemu_fprintf(f, "PC: 0x%08" PRIx32 "  SR: 0x%08" PRIx32
                 "  USP: 0x%08" PRIx32 "  SSP: 0x%08" PRIx32 "\n",
                 env->pc, env->sr, env->usp, env->ssp);
    qemu_fprintf(f, "VBR: 0x%08" PRIx32 "  PTBR: 0x%08" PRIx32
                 "  MMCR: 0x%08" PRIx32 "  halted=%u\n",
                 env->vbr, env->ptbr, env->mmcr, env->halted);
    qemu_fprintf(f, "CF=%u OF=%u IPL=%u S=%u\n",
                 !!(env->sr & MYEMU32_SR_CF), !!(env->sr & MYEMU32_SR_OF),
                 (env->sr & MYEMU32_SR_IPL_MASK) >> 2,
                 !!(env->sr & MYEMU32_SR_S));
}

static const TCGCPUOps myemulator32_tcg_ops = {
    .initialize = myemulator32_cpu_tcg_init,
    .synchronize_from_tb = myemulator32_sync_from_tb,
    .restore_state_to_opc = myemulator32_restore_state,
    .cpu_exec_interrupt = myemulator32_exec_interrupt,
    .cpu_exec_halt = myemulator32_has_work,
    .tlb_fill = myemulator32_cpu_tlb_fill,
    .do_interrupt = myemulator32_do_interrupt,
};

static void myemulator32_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);
    MyEmulator32CPUClass *mcc = MYEMULATOR32_CPU_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    device_class_set_parent_realize(dc, myemulator32_realize,
                                    &mcc->parent_realize);
    resettable_class_set_parent_phases(rc, NULL, myemulator32_reset_hold,
                                       NULL, &mcc->parent_phases);
    cc->class_by_name = myemulator32_class_by_name;
    cc->dump_state = myemulator32_cpu_dump_state;
    cc->has_work = myemulator32_has_work;
    cc->mmu_index = myemulator32_mmu_index;
    cc->set_pc = myemulator32_set_pc;
    cc->get_pc = myemulator32_get_pc;
    cc->sysemu_ops = &myemulator32_sysemu_ops;
    cc->gdb_num_core_regs = 0;
    cc->tcg_ops = &myemulator32_tcg_ops;
}

static const TypeInfo myemulator32_cpu_types[] = {
    {
        .name = TYPE_MYEMULATOR32_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(MyEmulator32CPU),
        .instance_align = __alignof__(MyEmulator32CPU),
        .class_size = sizeof(MyEmulator32CPUClass),
        .class_init = myemulator32_class_init,
        .abstract = true,
    },
    {
        .name = MYEMULATOR32_CPU_TYPE_NAME("myemu32"),
        .parent = TYPE_MYEMULATOR32_CPU,
    },
};

DEFINE_TYPES(myemulator32_cpu_types)
