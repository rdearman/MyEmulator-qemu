#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/boards.h"
#include "hw/core/cpu.h"
#include "hw/loader.h"
#include "hw/qdev-properties-system.h"
#include "hw/sysbus.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "qom/object.h"
#include "sysemu/block-backend.h"
#include "target/myemulator/cpu-qom.h"
#include "target/myemulator/cpu.h"
#include "myemulator-debug.h"
#include "myemulator-bootrom.h"
#include "myemulator-floppy.h"

#define TYPE_MYEMULATOR_MACHINE MACHINE_TYPE_NAME("myemulator")
#define MYEMULATOR_RAM_SIZE 0x10000

typedef struct MyEmulatorMachineState {
    MachineState parent_obj;
    MyEmulatorCPU *cpu;
    MemoryRegion ram;
} MyEmulatorMachineState;

DECLARE_INSTANCE_CHECKER(MyEmulatorMachineState, MYEMULATOR_MACHINE,
                         TYPE_MYEMULATOR_MACHINE)

static void myemulator_machine_init(MachineState *machine)
{
    MyEmulatorMachineState *s = MYEMULATOR_MACHINE(machine);
    MemoryRegion *sysmem = get_system_memory();

    s->cpu = MYEMULATOR_CPU(cpu_create(machine->cpu_type));
    myemulator_debug_register_qmp();

    memory_region_init_ram(&s->ram, NULL, "myemulator.ram",
                           MYEMULATOR_RAM_SIZE, &error_fatal);
    memory_region_add_subregion(sysmem, 0, &s->ram);

    BlockBackend *floppy = blk_by_name("myemulator-floppy");
    DeviceState *fdc = qdev_new(TYPE_MYEMULATOR_FLOPPY);
    if (floppy) {
        qdev_prop_set_drive(fdc, "drive", floppy);
    }
    sysbus_realize_and_unref(SYS_BUS_DEVICE(fdc), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(fdc), 0, MYEMULATOR_FLOPPY_BASE);

    if (floppy && !machine->kernel_filename) {
        rom_add_blob_fixed("myemulator.bootrom", myemulator_bootrom,
                           myemulator_bootrom_size, 0x0180);
    }

    if (machine->kernel_filename) {
        long size = load_image_targphys(machine->kernel_filename, 0,
                                        MYEMULATOR_RAM_SIZE);
        if (size < 0) {
            error_report("could not load kernel '%s'",
                         machine->kernel_filename);
            exit(1);
        }
    }

    /* Temporary bring-up image: little-endian reset vectors in RAM. */
    address_space_stw_le(&address_space_memory, 0xfffc, 0xffff,
                         MEMTXATTRS_UNSPECIFIED, NULL);
    address_space_stw_le(&address_space_memory, 0xfffe,
                         (floppy && !machine->kernel_filename) ? 0x0180 : 0x0000,
                         MEMTXATTRS_UNSPECIFIED, NULL);
    cpu_env(CPU(s->cpu))->sp = address_space_lduw_le(
        &address_space_memory, 0xfffc, MEMTXATTRS_UNSPECIFIED, NULL);
    cpu_env(CPU(s->cpu))->pc = address_space_lduw_le(
        &address_space_memory, 0xfffe, MEMTXATTRS_UNSPECIFIED, NULL);

    cpu_resume(CPU(s->cpu));

    /* Bring-up/test injection for asserted level-sensitive IRQ inputs. */
    const char *initial_sp = getenv("MYEMULATOR_INITIAL_SP");
    if (initial_sp != NULL) {
        char *end;
        unsigned long value = strtoul(initial_sp, &end, 0);

        if (*initial_sp != '\0' && *end == '\0') {
            cpu_env(CPU(s->cpu))->sp = value & 0xffff;
        }
    }
    const char *initial_s0 = getenv("MYEMULATOR_INITIAL_S0");
    if (initial_s0 != NULL) {
        char *end;
        unsigned long value = strtoul(initial_s0, &end, 0);

        if (*initial_s0 != '\0' && *end == '\0') {
            cpu_env(CPU(s->cpu))->s0 = value & 0xff;
        }
    }
    const char *irq_mask = getenv("MYEMULATOR_IRQ_MASK");
    if (irq_mask != NULL) {
        char *end;
        unsigned long mask = strtoul(irq_mask, &end, 0);

        if (*irq_mask != '\0' && *end == '\0') {
            myemulator_cpu_set_irq_mask(CPU(s->cpu), mask);
        }
    }
    const char *irq_oneshot = getenv("MYEMULATOR_IRQ_ONESHOT");
    if (irq_oneshot != NULL && strcmp(irq_oneshot, "0") != 0) {
        myemulator_cpu_set_irq_oneshot(CPU(s->cpu), true);
    }
    const char *irq_after = getenv("MYEMULATOR_IRQ_AFTER");
    if (irq_after != NULL) {
        char *end;
        unsigned long level = strtoul(irq_after, &end, 0);

        if (*irq_after != '\0' && *end == '\0') {
            myemulator_cpu_set_irq_after_entry(CPU(s->cpu), level);
        }
    }
}

static void myemulator_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "MyEmulator fictional 8-bit computer";
    mc->init = myemulator_machine_init;
    mc->default_cpu_type = MYEMULATOR_CPU_TYPE_NAME("myemu8");
    mc->default_ram_size = MYEMULATOR_RAM_SIZE;
    mc->default_cpus = 1;
    mc->min_cpus = 1;
    mc->max_cpus = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
}

static const TypeInfo myemulator_machine_types[] = {
    {
        .name = TYPE_MYEMULATOR_MACHINE,
        .parent = TYPE_MACHINE,
        .instance_size = sizeof(MyEmulatorMachineState),
        .class_init = myemulator_machine_class_init,
    },
};

DEFINE_TYPES(myemulator_machine_types)
