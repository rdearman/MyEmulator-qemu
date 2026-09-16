#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/core/boards.h"
#include "hw/core/cpu.h"
#include "hw/core/loader.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "qom/object.h"
#include "target/myemulator/cpu-qom.h"
#include "target/myemulator/cpu.h"

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

    memory_region_init_ram(&s->ram, NULL, "myemulator.ram",
                           MYEMULATOR_RAM_SIZE, &error_fatal);
    memory_region_add_subregion(sysmem, 0, &s->ram);

    if (machine->kernel_filename) {
        long size = load_image_targphys(machine->kernel_filename, 0,
                                        MYEMULATOR_RAM_SIZE, NULL);
        if (size < 0) {
            error_report("could not load kernel '%s'",
                         machine->kernel_filename);
            exit(1);
        }
    }

    /* Temporary bring-up image: little-endian reset vectors in RAM. */
    address_space_stw_le(&address_space_memory, 0xfffc, 0xffff,
                         MEMTXATTRS_UNSPECIFIED, NULL);
    address_space_stw_le(&address_space_memory, 0xfffe, 0x0000,
                         MEMTXATTRS_UNSPECIFIED, NULL);
    cpu_env(CPU(s->cpu))->sp = address_space_lduw_le(
        &address_space_memory, 0xfffc, MEMTXATTRS_UNSPECIFIED, NULL);
    cpu_env(CPU(s->cpu))->pc = address_space_lduw_le(
        &address_space_memory, 0xfffe, MEMTXATTRS_UNSPECIFIED, NULL);
    cpu_resume(CPU(s->cpu));
}

static void myemulator_machine_class_init(ObjectClass *oc, const void *data)
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
