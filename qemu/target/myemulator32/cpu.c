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
#include "qemu/main-loop.h"
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

static void myemulator32_do_interrupt(CPUState *cs);

uint64_t myemulator32_cpu_time_us(CPUMyEmulator32State *env)
{
    uint64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    return (now - env->time_base_ns) / 1000;
}

static void myemulator32_schedule_timer_irq(CPUMyEmulator32State *env)
{
    CPUState *cs = env_cpu(env);

    env->time_irq_asserted = true;
    myemulator32_cpu_set_irq(cs, 1, true);
}

void myemulator32_cpu_program_timecmp(CPUMyEmulator32State *env)
{
    uint64_t target;
    uint64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    if (!env->timecmp_armed) {
        timer_del(env->time_timer);
        return;
    }
    target = env->time_base_ns + env->timecmp * 1000;
    if (target <= now) {
        myemulator32_schedule_timer_irq(env);
        timer_del(env->time_timer);
    } else {
        timer_mod_ns(env->time_timer, target);
    }
}

static void myemulator32_time_expire(void *opaque)
{
    CPUMyEmulator32State *env = opaque;

    if (env->timecmp_armed &&
        myemulator32_cpu_time_us(env) >= env->timecmp) {
        myemulator32_schedule_timer_irq(env);
    } else {
        myemulator32_cpu_program_timecmp(env);
    }
}

static G_NORETURN void myemulator32_mmu_fault(CPUState *cs,
                                               CPUMyEmulator32State *env,
                                               uintptr_t retaddr,
                                               unsigned cause,
                                               vaddr address)
{
    cpu_restore_state(cs, retaddr);
    helper_exception(env, cause, env->pc, address);
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
        myemulator32_do_interrupt(cs);
        return true;
    }
    if (interrupt_request & CPU_INTERRUPT_HARD) {
        unsigned ipl = (env->sr & MYEMU32_SR_IPL_MASK) >> 2;

        for (unsigned level = 7; level > ipl; level--) {
            if (env->irq_asserted & (1u << level)) {
                cs->exception_index = MYEMU32_EXCP_IRQ;
                myemulator32_do_interrupt(cs);
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
    bool allowed;
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
            myemulator32_mmu_fault(cs, env, retaddr, fault_page, address);
        }
        return false;
    }
    if (pde & 0xf80) {
        if (!probe) {
            myemulator32_mmu_fault(cs, env, retaddr, fault_prot, address);
        }
        return false;
    }

    pte_addr = (pde & 0xfffff000u) + pte_index * sizeof(uint32_t);
    pte = address_space_ldl_le(&address_space_memory, pte_addr,
                               MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK || !(pte & 1)) {
        if (!probe) {
            myemulator32_mmu_fault(cs, env, retaddr, fault_page, address);
        }
        return false;
    }
    if (pte & 0xf80) {
        if (!probe) {
            myemulator32_mmu_fault(cs, env, retaddr, fault_prot, address);
        }
        return false;
    }

    /* The Linux PTE format uses the low flag bits directly: USER=bit 1,
     * READ=bit 2, WRITE=bit 3, EXEC=bit 4.  Check those flags directly
     * instead of deriving a second, shifted permission namespace.  This
     * keeps the hardware permission test identical to the documented PTE
     * format and avoids rejecting executable user pages after a demand
     * fault has installed them. */
    allowed = !user || ((pde & (1u << 1)) && (pte & (1u << 1)));
    if (allowed && fetch) {
        allowed = (pde & (1u << 4)) && (pte & (1u << 4));
    } else if (allowed && store) {
        allowed = (pde & (1u << 3)) && (pte & (1u << 3));
    } else if (allowed) {
        allowed = (pde & (1u << 2)) && (pte & (1u << 2));
    }
    if (!allowed) {
        if (!probe) {
            myemulator32_mmu_fault(cs, env, retaddr, fault_prot, address);
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
            myemulator32_mmu_fault(cs, env, retaddr, fault_prot, address);
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
        bool need_bql = !bql_locked();
        if (need_bql) {
            bql_lock();
        }
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
        cpu_exit(cs);
        if (need_bql) {
            bql_unlock();
        }
    } else {
        env->irq_asserted &= ~(1u << level);
        if (!env->irq_asserted) {
            bool need_bql = !bql_locked();
            if (need_bql) {
                bql_lock();
            }
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
            if (need_bql) {
                bql_unlock();
            }
        }
    }
}

void myemulator32_cpu_set_nmi(CPUState *cs, bool asserted)
{
    if (asserted) {
        bool need_bql = !bql_locked();
        if (need_bql) {
            bql_lock();
        }
        cpu_interrupt(cs, CPU_INTERRUPT_NMI);
        cpu_exit(cs);
        if (need_bql) {
            bql_unlock();
        }
    } else {
        bool need_bql = !bql_locked();
        if (need_bql) {
            bql_lock();
        }
        cpu_reset_interrupt(cs, CPU_INTERRUPT_NMI);
        if (need_bql) {
            bql_unlock();
        }
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
    QEMUTimer *time_timer = env->time_timer;
    if (time_timer) {
        timer_del(time_timer);
    }
    memset(env, 0, sizeof(*env));
    env->time_timer = time_timer;
    cs->halted = 0;
    cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD | CPU_INTERRUPT_NMI);
    env->time_base_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
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

static void myemulator32_init(Object *obj)
{
    MyEmulator32CPU *cpu = MYEMULATOR32_CPU(obj);

    cpu->env.time_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                       myemulator32_time_expire,
                                       &cpu->env);
}

static void myemulator32_finalize(Object *obj)
{
    MyEmulator32CPU *cpu = MYEMULATOR32_CPU(obj);

    timer_del(cpu->env.time_timer);
    timer_free(cpu->env.time_timer);
    cpu->env.time_timer = NULL;
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
                 "  MMCR: 0x%08" PRIx32 "  DFSP: 0x%08" PRIx32
                 "  TP: 0x%08" PRIx32 "  TIME: 0x%016" PRIx64
                 "  TIMECMP: 0x%016" PRIx64 "  halted=%u\n",
                 env->vbr, env->ptbr, env->mmcr, env->dfsp, env->tp,
                 myemulator32_cpu_time_us(env), env->timecmp,
                 env->halted);
    qemu_fprintf(f, "IRQ_ASSERTED=0x%02" PRIx8 " IRQ_REQUEST=0x%08" PRIx32 "\n",
                 env->irq_asserted, cs->interrupt_request);
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
        .instance_init = myemulator32_init,
        .instance_finalize = myemulator32_finalize,
        .class_init = myemulator32_class_init,
        .abstract = true,
    },
    {
        .name = MYEMULATOR32_CPU_TYPE_NAME("myemu32"),
        .parent = TYPE_MYEMULATOR32_CPU,
    },
};

DEFINE_TYPES(myemulator32_cpu_types)
