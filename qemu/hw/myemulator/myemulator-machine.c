#include "qemu/osdep.h"
#include "qapi/error.h"
#include "chardev/char.h"
#include "qemu/error-report.h"
#include "hw/boards.h"
#include "hw/core/cpu.h"
#include "hw/qdev-properties-system.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "qom/object.h"
#include "sysemu/block-backend.h"
#include "target/myemulator/cpu-qom.h"
#include "target/myemulator/cpu.h"
#include "myemulator-debug.h"
#include "myemulator-floppy.h"
#include "myemulator-console.h"
#include "myemulator-timer.h"

#define TYPE_MYEMULATOR_MACHINE MACHINE_TYPE_NAME("myemulator")
#define MYEMULATOR_RAM_SIZE 0xf000
#define MYEMULATOR_FIRMWARE_BASE 0xf100
#define MYEMULATOR_FIRMWARE_SIZE 0x0f00

typedef struct MyEmulatorMachineState {
    MachineState parent_obj;
    MyEmulatorCPU *cpu;
    MemoryRegion ram;
    MemoryRegion firmware;
} MyEmulatorMachineState;

static void myemulator_machine_irq(void *opaque, int number, int level)
{
    myemulator_cpu_set_irq(CPU(opaque), number, level != 0);
}

static void myemulator_add_firmware(MyEmulatorMachineState *s,
                                    MachineState *machine,
                                    const gchar *kernel_data,
                                    gsize kernel_size)
{
    uint8_t *image = g_malloc(MYEMULATOR_FIRMWARE_SIZE);
    gsize file_size = 0;
    gchar *file_data = NULL;
    GError *error = NULL;

    memset(image, 0xff, MYEMULATOR_FIRMWARE_SIZE);

    if (machine->firmware) {
        if (!g_file_get_contents(machine->firmware, &file_data, &file_size,
                                 &error)) {
            error_report("could not read firmware '%s': %s",
                         machine->firmware, error->message);
            g_error_free(error);
            g_free(image);
            exit(1);
        }
        if (file_size > MYEMULATOR_FIRMWARE_SIZE) {
            error_report("firmware '%s' is %zu bytes; maximum is %u bytes",
                         machine->firmware, file_size,
                         MYEMULATOR_FIRMWARE_SIZE);
            g_free(file_data);
            g_free(image);
            exit(1);
        }
        memcpy(image, file_data, file_size);
        g_free(file_data);
    } else if (machine->kernel_filename) {
        /* Development -kernel mode gets ROM-owned vectors.  A legacy full
         * 64 KiB raw image may supply its top ROM window explicitly. */
        if (kernel_size == 0x10000) {
            memcpy(image, kernel_data + MYEMULATOR_FIRMWARE_BASE,
                   MYEMULATOR_FIRMWARE_SIZE);
        } else {
            image[0xfffc - MYEMULATOR_FIRMWARE_BASE] = 0x00;
            image[0xfffd - MYEMULATOR_FIRMWARE_BASE] = 0xf0;
            image[0xfffe - MYEMULATOR_FIRMWARE_BASE] = 0x00;
            image[0xffff - MYEMULATOR_FIRMWARE_BASE] = 0x00;
        }
    } else {
        error_report("no firmware image supplied; use -bios FIRMWARE or -kernel IMAGE");
        g_free(image);
        exit(1);
    }

    memory_region_init_rom(&s->firmware, NULL, "myemulator.firmware",
                           MYEMULATOR_FIRMWARE_SIZE, &error_fatal);
    memcpy(memory_region_get_ram_ptr(&s->firmware), image,
           MYEMULATOR_FIRMWARE_SIZE);
    memory_region_add_subregion(get_system_memory(), MYEMULATOR_FIRMWARE_BASE,
                                &s->firmware);
    g_free(image);
}

DECLARE_INSTANCE_CHECKER(MyEmulatorMachineState, MYEMULATOR_MACHINE,
                         TYPE_MYEMULATOR_MACHINE)

static void myemulator_machine_init(MachineState *machine)
{
    MyEmulatorMachineState *s = MYEMULATOR_MACHINE(machine);
    MemoryRegion *sysmem = get_system_memory();
    gchar *kernel_data = NULL;
    gsize kernel_size = 0;
    GError *kernel_error = NULL;

    s->cpu = MYEMULATOR_CPU(cpu_create(machine->cpu_type));
    myemulator_debug_register_qmp();

    memory_region_init_ram(&s->ram, NULL, "myemulator.ram",
                           MYEMULATOR_RAM_SIZE, &error_fatal);
    memory_region_add_subregion(sysmem, 0, &s->ram);

    if (machine->kernel_filename) {
        if (!g_file_get_contents(machine->kernel_filename, &kernel_data,
                                 &kernel_size, &kernel_error)) {
            error_report("could not read kernel '%s': %s",
                         machine->kernel_filename, kernel_error->message);
            g_error_free(kernel_error);
            exit(1);
        }
        if (kernel_size > 0x10000 ||
            (kernel_size > MYEMULATOR_RAM_SIZE && kernel_size != 0x10000)) {
            error_report("kernel '%s' must be at most 0x%04x bytes, or exactly 0x10000 bytes",
                         machine->kernel_filename, MYEMULATOR_RAM_SIZE);
            g_free(kernel_data);
            exit(1);
        }
    }

    BlockBackend *floppy = blk_by_name("myemulator-floppy");
    DeviceState *fdc = qdev_new(TYPE_MYEMULATOR_FLOPPY);
    if (floppy) {
        qdev_prop_set_drive(fdc, "drive", floppy);
    }
    sysbus_realize_and_unref(SYS_BUS_DEVICE(fdc), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(fdc), 0, MYEMULATOR_FLOPPY_BASE);

    Chardev *console_chr = qemu_chr_find("console");
    DeviceState *console = qdev_new(TYPE_MYEMULATOR_CONSOLE);
    if (console_chr) {
        qdev_prop_set_chr(console, "chardev", console_chr);
    }

    DeviceState *timer = qdev_new(TYPE_MYEMULATOR_TIMER);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(timer), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(timer), 0, MYEMULATOR_TIMER_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(timer), 0,
                       qemu_allocate_irq(myemulator_machine_irq, s->cpu,
                                         MYEMULATOR_TIMER_IRQ));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(console), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(console), 0, MYEMULATOR_CONSOLE_BASE);
    if (console_chr) {
        sysbus_connect_irq(SYS_BUS_DEVICE(console), 0,
                           qemu_allocate_irq(myemulator_machine_irq, s->cpu,
                                             MYEMULATOR_CONSOLE_IRQ));
    }

    if (machine->firmware && machine->kernel_filename) {
        error_report("-bios and -kernel cannot be used together on myemulator");
        exit(1);
    }

    myemulator_add_firmware(s, machine, kernel_data, kernel_size);

    if (machine->kernel_filename) {
        memcpy(memory_region_get_ram_ptr(&s->ram), kernel_data,
               MIN(kernel_size, (gsize)MYEMULATOR_RAM_SIZE));
        g_free(kernel_data);
    }

    /* Reset vectors are owned by the firmware ROM (or the -kernel
     * compatibility ROM image), never manufactured in writable RAM. */
    cpu_env(CPU(s->cpu))->sp = address_space_lduw_le(
        &address_space_memory, 0xfffc, MEMTXATTRS_UNSPECIFIED, NULL);
    cpu_env(CPU(s->cpu))->pc = address_space_lduw_le(
        &address_space_memory, 0xfffe, MEMTXATTRS_UNSPECIFIED, NULL);
    if (cpu_env(CPU(s->cpu))->pc & 1) {
        error_report("MyEmulator reset PC vector is odd: 0x%04x; CPU halted",
                     cpu_env(CPU(s->cpu))->pc & 0xffff);
        cpu_env(CPU(s->cpu))->halted = true;
        CPU(s->cpu)->halted = 1;
    }

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
