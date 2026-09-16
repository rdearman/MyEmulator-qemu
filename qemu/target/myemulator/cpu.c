#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "exec/translation-block.h"
#include "accel/tcg/cpu-ops.h"

static void myemulator_cpu_set_pc(CPUState *cs, vaddr value)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    env->pc = value & 0xffff;
}

static vaddr myemulator_cpu_get_pc(CPUState *cs)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    return env->pc;
}

static TCGTBCPUState myemulator_get_tb_cpu_state(CPUState *cs)
{
    CPUMyEmulatorState *env = cpu_env(cs);

    return (TCGTBCPUState){ .pc = env->pc, .flags = 0 };
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
    return false;
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

    cpu_common_realize(cs, &local_err);
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
    qemu_fprintf(f, "LR: 0x%02x\n", env->lr & 0xff);
    qemu_fprintf(f, "R0: 0x%02x  R1: 0x%02x  R2: 0x%02x  R3: 0x%02x\n",
                 env->r[0] & 0xff, env->r[1] & 0xff,
                 env->r[2] & 0xff, env->r[3] & 0xff);
    qemu_fprintf(f, "FLAGS: ZF=%u OF=%u CF=%u\n",
                 env->zf & 1, env->of & 1, env->cf & 1);
}

#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps myemulator_sysemu_ops = {
    .has_work = myemulator_cpu_has_work,
    .get_phys_addr_debug = myemulator_cpu_get_phys_addr_debug,
};

static const TCGCPUOps myemulator_tcg_ops = {
    .initialize = myemulator_cpu_tcg_init,
    .translate_code = myemulator_translate_code,
    .get_tb_cpu_state = myemulator_get_tb_cpu_state,
    .synchronize_from_tb = myemulator_cpu_synchronize_from_tb,
    .restore_state_to_opc = myemulator_restore_state_to_opc,
    .mmu_index = myemulator_cpu_mmu_index,
    .cpu_exec_interrupt = myemulator_cpu_exec_interrupt,
    .cpu_exec_reset = cpu_reset,
    .cpu_exec_halt = myemulator_cpu_has_work,
    .tlb_fill = myemulator_cpu_tlb_fill,
    .do_interrupt = myemulator_cpu_do_interrupt,
    .pointer_wrap = cpu_pointer_wrap_uint32,
};

static void myemulator_cpu_class_init(ObjectClass *oc, const void *data)
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
    cc->set_pc = myemulator_cpu_set_pc;
    cc->get_pc = myemulator_cpu_get_pc;
    cc->sysemu_ops = &myemulator_sysemu_ops;
    cc->gdb_read_register = myemulator_cpu_gdb_read_register;
    cc->gdb_write_register = myemulator_cpu_gdb_write_register;
    cc->gdb_num_core_regs = 8;
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
