#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/address-spaces.h"
#include "exec/cpu-all.h"
#include "exec/exec-all.h"
#include "exec/cputlb.h"
#include "exec/helper-proto.h"
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
    return (cpu_env(cs)->sr & MYEMU32_SR_S) ?
           MMU_SUPERVISOR_IDX : MMU_USER_IDX;
}

static bool myemulator32_has_work(CPUState *cs)
{
    CPUMyEmulator32State *env = cpu_env(cs);
    unsigned ipl = (env->sr & MYEMU32_SR_IPL_MASK) >> 2;
    bool irq = false;

    for (unsigned level = 1; level <= 7; level++) {
        if (level > ipl && (env->irq_asserted & (1u << level))) {
            irq = true;
        }
    }
    return !env->halted || irq ||
           ((cs->interrupt_request & CPU_INTERRUPT_NMI) &&
            !env->nmi_active);
}

static bool myemulator32_exec_interrupt(CPUState *cs, int interrupt_request)
{
    CPUMyEmulator32State *env = cpu_env(cs);

    if (interrupt_request & CPU_INTERRUPT_EXITTB) {
        cpu_reset_interrupt(cs, CPU_INTERRUPT_EXITTB);
        return true;
    }
    if ((interrupt_request & CPU_INTERRUPT_NMI) && !env->nmi_active) {
        cs->exception_index = MYEMU32_EXCP_NMI;
        return true;
    }
    if (interrupt_request & CPU_INTERRUPT_HARD) {
        unsigned ipl = (env->sr & MYEMU32_SR_IPL_MASK) >> 2;

        for (unsigned level = 7; level > ipl; level--) {
            if (env->irq_asserted & (1u << level)) {
                cs->exception_index = MYEMU32_EXCP_IRQ;
                return true;
            }
        }
    }
    return false;
}

static void myemulator32_do_interrupt(CPUState *cs)
{
    CPUMyEmulator32State *env = cpu_env(cs);

    if (cs->exception_index == MYEMU32_EXCP_IRQ) {
        unsigned ipl = (env->sr & MYEMU32_SR_IPL_MASK) >> 2;

        for (unsigned level = 7; level > ipl; level--) {
            if (env->irq_asserted & (1u << level)) {
                helper_irq(env, level);
                return;
            }
        }
    } else if (cs->exception_index == MYEMU32_EXCP_NMI) {
        helper_nmi(env);
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
    CPUMyEmulator32State *env = cpu_env(cs);
    hwaddr page = address & TARGET_PAGE_MASK;
    uint32_t pde, pte;
    uint32_t pde_addr, pte_addr;
    uint32_t perms;
    uint32_t pde_index = (address >> 22) & 0x3ff;
    uint32_t pte_index = (address >> 12) & 0x3ff;
    MemTxResult result = MEMTX_OK;
    bool user = mmu_idx == MMU_USER_IDX;
    bool store = access_type == MMU_DATA_STORE;
    bool fetch = access_type == MMU_INST_FETCH;
    unsigned fault_page, fault_prot;

    if (!env->mmcr) {
        tlb_set_page(cs, page, page, PAGE_READ | PAGE_WRITE | PAGE_EXEC,
                     mmu_idx, TARGET_PAGE_SIZE);
        return true;
    }

    pde_addr = env->ptbr + pde_index * sizeof(uint32_t);
    pde = address_space_ldl_le(&address_space_memory, pde_addr,
                               MEMTXATTRS_UNSPECIFIED, &result);
    fault_page = fetch ? MYEMU32_VECTOR_INSN_PAGE :
                 (store ? MYEMU32_VECTOR_STORE_PAGE :
                          MYEMU32_VECTOR_LOAD_PAGE);
    fault_prot = fetch ? MYEMU32_VECTOR_INSN_PROT :
                 (store ? MYEMU32_VECTOR_STORE_PROT :
                          MYEMU32_VECTOR_LOAD_PROT);
    if (result != MEMTX_OK || !(pde & 1)) {
        if (!probe) {
            helper_exception(env, fault_page, env->pc, address);
        }
        return false;
    }
    if (pde & 0xf80) {
        if (!probe) {
            helper_exception(env, fault_prot, env->pc, address);
        }
        return false;
    }

    pte_addr = (pde & 0xfffff000u) + pte_index * sizeof(uint32_t);
    pte = address_space_ldl_le(&address_space_memory, pte_addr,
                               MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK || !(pte & 1)) {
        if (!probe) {
            helper_exception(env, fault_page, env->pc, address);
        }
        return false;
    }
    if (pte & 0xf80) {
        if (!probe) {
            helper_exception(env, fault_prot, env->pc, address);
        }
        return false;
    }

    perms = (pde & pte) >> 1;
    if ((user && !(pde & 2) ) || (user && !(pte & 2)) ||
        (fetch ? !(perms & (1u << 3)) :
         store ? !(perms & (1u << 2)) : !(perms & (1u << 1)))) {
        if (!probe) {
            helper_exception(env, fault_prot, env->pc, address);
        }
        return false;
    }

    pde |= 1u << 5;
    pte |= 1u << 5;
    if (store) {
        pde |= 1u << 6;
        pte |= 1u << 6;
    }
    address_space_stl_le(&address_space_memory, pde_addr, pde,
                         MEMTXATTRS_UNSPECIFIED, &result);
    address_space_stl_le(&address_space_memory, pte_addr, pte,
                         MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK) {
        if (!probe) {
            helper_exception(env, fault_prot, env->pc, address);
        }
        return false;
    }

    perms = 0;
    if ((pde & pte & (1u << 2)) != 0) {
        perms |= PAGE_READ;
    }
    if ((pde & pte & (1u << 3)) != 0) {
        perms |= PAGE_WRITE;
    }
    if ((pde & pte & (1u << 4)) != 0) {
        perms |= PAGE_EXEC;
    }
    tlb_set_page(cs, page, pte & 0xfffff000u,
                 perms, mmu_idx, TARGET_PAGE_SIZE);
    return true;
}

void myemulator32_cpu_set_irq(CPUState *cs, unsigned level, bool asserted)
{
    CPUMyEmulator32State *env = cpu_env(cs);

    if (level == 0 || level > 7) {
        return;
    }
    if (asserted) {
        env->irq_asserted |= 1u << level;
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        env->irq_asserted &= ~(1u << level);
        if (!env->irq_asserted) {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
        }
    }
}

void myemulator32_cpu_set_nmi(CPUState *cs, bool asserted)
{
    if (asserted) {
        cpu_interrupt(cs, CPU_INTERRUPT_NMI);
    } else {
        cpu_reset_interrupt(cs, CPU_INTERRUPT_NMI);
    }
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
    cs->halted = 0;
    cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD | CPU_INTERRUPT_NMI);
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
