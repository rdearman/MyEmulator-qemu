#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/translation-block.h"
#include "hw/core/tcg-cpu-ops.h"

static void myemulator_cpu_set_pc(CPUState *cs, vaddr value)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    env->pc = value & 0xffff;
}

static void myemulator_cpu_disas_set_info(CPUState *cpu,
                                          disassemble_info *info)
{
    info->print_insn = myemulator_print_insn;
}

static vaddr myemulator_cpu_get_pc(CPUState *cs)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    return env->pc;
}

static void myemulator_cpu_synchronize_from_tb(CPUState *cs,
                                               const TranslationBlock *tb)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    env->pc = tb->pc;
}

static void myemulator_restore_state_to_opc(CPUState *cs,
                                            const TranslationBlock *tb,
                                            const uint64_t *data)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    env->pc = data[0];
}

static int myemulator_cpu_mmu_index(CPUState *cs, bool ifetch)
{
    return MMU_PHYS_IDX;
}

static bool myemulator_cpu_has_work(CPUState *cs)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    return !env->halted || myemulator_cpu_highest_irq(env) != 0;
}

static void myemulator_cpu_reset_hold(Object *obj, ResetType type)
{
    CPUState *cs = CPU(obj);
    MyEmulatorCPU *cpu = MYEMULATOR_CPU(cs);
    MyEmulatorCPUClass *mcc = MYEMULATOR_CPU_GET_CLASS(obj);
    CPUMyEmulatorState *env = &cpu->env;

    if (mcc->parent_phases.hold) {
        mcc->parent_phases.hold(obj, type);
    }

    memset(env, 0, sizeof(*env));
    env->sp = 0xffff;
    cs->exception_index = -1;
}

static void myemulator_cpu_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    MyEmulatorCPUClass *mcc = MYEMULATOR_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }
    qemu_init_vcpu(cs);
    cpu_reset(cs);
    mcc->parent_realize(dev, errp);
}

static ObjectClass *myemulator_cpu_class_by_name(const char *cpu_model)
{
    g_autofree char *typename = g_strdup_printf(MYEMULATOR_CPU_TYPE_NAME("%s"),
                                                cpu_model);

    return object_class_by_name(typename);
}

static void myemulator_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    qemu_fprintf(f, "PC: 0x%04x\n", env->pc & 0xffff);
    qemu_fprintf(f, "SP: 0x%04x\n", env->sp & 0xffff);
    qemu_fprintf(f, "LR: 0x%04x\n", env->lr & 0xffff);
    qemu_fprintf(f, "R0: 0x%02x  R1: 0x%02x  R2: 0x%02x  R3: 0x%02x\n",
                 env->r[0] & 0xff, env->r[1] & 0xff,
                 env->r[2] & 0xff, env->r[3] & 0xff);
    qemu_fprintf(f, "A0: 0x%04x  A1: 0x%04x  A2: 0x%04x  A3: 0x%04x\n",
                 env->a[0] & 0xffff, env->a[1] & 0xffff,
                 env->a[2] & 0xffff, env->a[3] & 0xffff);
    qemu_fprintf(f, "S0: 0x%02x\n", env->s0 & 0xff);
    qemu_fprintf(f, "FLAGS: ZF=%u NF=%u OF=%u CF=%u\n",
                 !!(env->s0 & MYEMULATOR_S0_ZF),
                 !!(env->s0 & MYEMULATOR_S0_NF),
                 !!(env->s0 & MYEMULATOR_S0_OF),
                 !!(env->s0 & MYEMULATOR_S0_CF));
}

#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps myemulator_sysemu_ops = {
    .get_phys_page_debug = myemulator_cpu_get_phys_addr_debug,
};

static const TCGCPUOps myemulator_tcg_ops = {
    .initialize = myemulator_cpu_tcg_init,
    .synchronize_from_tb = myemulator_cpu_synchronize_from_tb,
    .restore_state_to_opc = myemulator_restore_state_to_opc,
    .cpu_exec_interrupt = myemulator_cpu_exec_interrupt,
    .debug_check_breakpoint = myemulator_cpu_debug_check_breakpoint,
    .cpu_exec_halt = myemulator_cpu_has_work,
    .tlb_fill = myemulator_cpu_tlb_fill,
    .do_interrupt = myemulator_cpu_do_interrupt,
};

static void myemulator_cpu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);
    MyEmulatorCPUClass *mcc = MYEMULATOR_CPU_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    device_class_set_parent_realize(dc, myemulator_cpu_realizefn,
                                    &mcc->parent_realize);
    resettable_class_set_parent_phases(rc, NULL, myemulator_cpu_reset_hold,
                                       NULL, &mcc->parent_phases);

    cc->class_by_name = myemulator_cpu_class_by_name;
    cc->dump_state = myemulator_cpu_dump_state;
    cc->has_work = myemulator_cpu_has_work;
    cc->mmu_index = myemulator_cpu_mmu_index;
    cc->set_pc = myemulator_cpu_set_pc;
    cc->get_pc = myemulator_cpu_get_pc;
    cc->sysemu_ops = &myemulator_sysemu_ops;
    cc->gdb_read_register = myemulator_cpu_gdb_read_register;
    cc->gdb_write_register = myemulator_cpu_gdb_write_register;
    cc->gdb_num_core_regs = 0;
    cc->gdb_core_xml_file = "myemulator-core.xml";
    cc->disas_set_info = myemulator_cpu_disas_set_info;
    cc->tcg_ops = &myemulator_tcg_ops;
}

static void myemu8_initfn(Object *obj)
{
}

#define DEFINE_MYEMULATOR_CPU_TYPE(model, initfn) \
    { \
        .parent = TYPE_MYEMULATOR_CPU, \
        .instance_init = initfn, \
        .name = MYEMULATOR_CPU_TYPE_NAME(model), \
    }

static const TypeInfo myemulator_cpu_type_info[] = {
    {
        .name = TYPE_MYEMULATOR_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(MyEmulatorCPU),
        .instance_align = __alignof__(MyEmulatorCPU),
        .class_size = sizeof(MyEmulatorCPUClass),
        .class_init = myemulator_cpu_class_init,
        .abstract = true,
    },
    DEFINE_MYEMULATOR_CPU_TYPE("myemu8", myemu8_initfn),
};

DEFINE_TYPES(myemulator_cpu_type_info)
