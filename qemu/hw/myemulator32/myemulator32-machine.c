#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/boards.h"
#include "hw/core/cpu.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "target/myemulator32/cpu-qom.h"
#include "target/myemulator32/cpu.h"

#define TYPE_MYEMULATOR32_MACHINE MACHINE_TYPE_NAME("myemulator32")
#define MYEMU32_DEFAULT_RAM (16 * 1024 * 1024)

typedef struct MyEmulator32MachineState {
    MachineState parent_obj;
    MyEmulator32CPU *cpu;
    MemoryRegion ram;
} MyEmulator32MachineState;

DECLARE_INSTANCE_CHECKER(MyEmulator32MachineState, MYEMULATOR32_MACHINE,
                         TYPE_MYEMULATOR32_MACHINE)

static void myemulator32_machine_init(MachineState *machine)
{
    MyEmulator32MachineState *s = MYEMULATOR32_MACHINE(machine);
    g_autofree gchar *image = NULL;
    gsize image_size = 0;
    g_autoptr(GError) error = NULL;

    if (!machine->kernel_filename) {
        error_report("myemulator32 requires -kernel IMAGE");
        exit(1);
    }
    if (!g_file_get_contents(machine->kernel_filename, &image, &image_size,
                             &error)) {
        error_report("could not read kernel '%s': %s", machine->kernel_filename,
                     error->message);
        exit(1);
    }
    if (image_size < 8 || image_size > machine->ram_size) {
        error_report("kernel '%s' must be between 8 bytes and 0x%" PRIx64
                     " bytes", machine->kernel_filename, machine->ram_size);
        exit(1);
    }

    memory_region_init_ram(&s->ram, NULL, "myemulator32.ram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), 0, &s->ram);
    memcpy(memory_region_get_ram_ptr(&s->ram), image, image_size);

    s->cpu = MYEMULATOR32_CPU(cpu_create(machine->cpu_type));
    cpu_reset(CPU(s->cpu));
    cpu_resume(CPU(s->cpu));
}

static void myemulator32_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "MyEmulator 2.0 32-bit computer (initial CPU milestone)";
    mc->init = myemulator32_machine_init;
    mc->default_cpu_type = MYEMULATOR32_CPU_TYPE_NAME("myemu32");
    mc->default_ram_size = MYEMU32_DEFAULT_RAM;
    mc->default_cpus = 1;
    mc->min_cpus = 1;
    mc->max_cpus = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
}

static const TypeInfo myemulator32_machine_types[] = {
    {
        .name = TYPE_MYEMULATOR32_MACHINE,
        .parent = TYPE_MACHINE,
        .instance_size = sizeof(MyEmulator32MachineState),
        .class_init = myemulator32_machine_class_init,
    },
};

DEFINE_TYPES(myemulator32_machine_types)
