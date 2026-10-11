#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/virtio/virtio-mmio.h"
#include "hw/virtio/virtio-net.h"
#include "net/net.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/core/cpu.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "target/myemulator32/cpu-qom.h"
#include "target/myemulator32/cpu.h"
#include "myemulator32-debug.h"
#include "myemulator32-console.h"
#include "myemulator32-block.h"
#include "myemulator32-power.h"
#include "chardev/char.h"
#include "sysemu/sysemu.h"
#include "sysemu/block-backend.h"
#include "exec/cpu-common.h"
#include "qemu/timer.h"

#define TYPE_MYEMULATOR32_MACHINE MACHINE_TYPE_NAME("myemulator32")
#define MYEMU32_DEFAULT_RAM (16 * 1024 * 1024)
#define MYEMU32_ELF_MACHINE 0xF2E2
#define MYEMU32_RESET_SSP_ADDR 0x00000400
#define MYEMU32_RESET_PC_ADDR  0x00000404
#define MYEMU32_BOOT_EPOCH_LO_ADDR 0x00000408
#define MYEMU32_BOOT_EPOCH_HI_ADDR 0x0000040c
#define MYEMU32_BOOT_RAM_ADDR  0x00000410
#define MYEMU32_BOOT_CMDLINE_PTR 0x00000414
#define MYEMU32_BOOT_CMDLINE_ADDR 0x00000600
#define MYEMU32_VIRTIO_NET_BASE 0xf0200000u
#define MYEMU32_VIRTIO_NET_IRQ  5u

typedef struct MyEmulator32MachineState {
    MachineState parent_obj;
    MyEmulator32CPU *cpu;
    MemoryRegion ram;
} MyEmulator32MachineState;

DECLARE_INSTANCE_CHECKER(MyEmulator32MachineState, MYEMULATOR32_MACHINE,
                         TYPE_MYEMULATOR32_MACHINE)

static void myemulator32_write_boot_epoch(void)
{
    uint64_t epoch_sec =
        (uint64_t)(qemu_clock_get_ns(QEMU_CLOCK_HOST) /
                   NANOSECONDS_PER_SECOND);

    stl_le_phys(&address_space_memory, MYEMU32_BOOT_EPOCH_LO_ADDR,
                (uint32_t)epoch_sec);
    stl_le_phys(&address_space_memory, MYEMU32_BOOT_EPOCH_HI_ADDR,
                (uint32_t)(epoch_sec >> 32));
}

static void myemulator32_machine_irq(void *opaque, int number, int level)
{
    myemulator32_cpu_set_irq(CPU(opaque), number, level != 0);
}

static void myemulator32_create_network(MyEmulator32MachineState *s)
{
    DeviceState *transport;
    DeviceState *net;
    VirtIOMMIOProxy *proxy;

    NetClientState *backend = qemu_find_netdev("net0");
    if (!backend) {
        return;
    }
    net = qdev_new(TYPE_VIRTIO_NET);
    qdev_prop_set_netdev(net, "netdev", backend);

    transport = qdev_new(TYPE_VIRTIO_MMIO);
    qdev_prop_set_bit(transport, "force-legacy", true);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(transport), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(transport), 0, MYEMU32_VIRTIO_NET_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(transport), 0,
                       qemu_allocate_irq(myemulator32_machine_irq,
                                         s->cpu, MYEMU32_VIRTIO_NET_IRQ));

    proxy = VIRTIO_MMIO(transport);
    qdev_realize_and_unref(net, BUS(&proxy->bus), &error_fatal);
}

static void myemulator32_machine_init(MachineState *machine)
{
    MyEmulator32MachineState *s = MYEMULATOR32_MACHINE(machine);
    g_autofree gchar *image = NULL;
    gsize image_size = 0;
    g_autoptr(GError) error = NULL;
    uint64_t elf_entry = 0;
    ssize_t loaded;
    bool is_elf = false;

    if (!machine->kernel_filename) {
        error_report("myemulator32 requires -kernel IMAGE");
        exit(1);
    }
    memory_region_init_ram(&s->ram, NULL, "myemulator32.ram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(get_system_memory(), 0, &s->ram);

    /* ELF32 is the canonical development image.  The generic loader checks
     * the ELF class, endianness and machine ID for us and loads PT_LOAD
     * segments, including zero-filling the BSS portion. */
    loaded = load_elf(machine->kernel_filename, NULL, NULL, NULL,
                      &elf_entry, NULL, NULL, NULL, 0,
                      MYEMU32_ELF_MACHINE, 0, 0);
    if (loaded >= 0) {
        uint32_t initial_ssp = machine->ram_size - 0x1000;
        stl_le_phys(&address_space_memory, MYEMU32_RESET_SSP_ADDR,
                    initial_ssp);
        stl_le_phys(&address_space_memory, MYEMU32_RESET_PC_ADDR,
                    (uint32_t)elf_entry);
        /* The early REM Linux port has no platform firmware or device tree.
         * Publish the selected QEMU RAM size beside the reset vector so the
         * kernel can use more than the historical 16 MiB default. */
        stl_le_phys(&address_space_memory, MYEMU32_BOOT_RAM_ADDR,
                    (uint32_t)machine->ram_size);
        if (machine->kernel_cmdline && machine->kernel_cmdline[0]) {
            size_t n = strlen(machine->kernel_cmdline) + 1;
            if (MYEMU32_BOOT_CMDLINE_ADDR + n >= machine->ram_size) {
                error_report("MyEmulator32 kernel command line is too large");
            } else {
                cpu_physical_memory_write(MYEMU32_BOOT_CMDLINE_ADDR,
                                          machine->kernel_cmdline, n);
                stl_le_phys(&address_space_memory,
                            MYEMU32_BOOT_CMDLINE_PTR,
                            MYEMU32_BOOT_CMDLINE_ADDR);
            }
        }
        is_elf = true;
    } else if (loaded != ELF_LOAD_NOT_ELF) {
        error_report("could not load MyEmulator2 ELF '%s': %s",
                     machine->kernel_filename, load_elf_strerror(loaded));
        exit(1);
    }

    if (!is_elf) {
        if (!g_file_get_contents(machine->kernel_filename, &image, &image_size,
                                 &error)) {
            error_report("could not read kernel '%s': %s",
                         machine->kernel_filename, error->message);
            exit(1);
        }
        if (image_size < 8 || image_size > machine->ram_size) {
            error_report("kernel '%s' must be between 8 bytes and 0x%" PRIx64
                         " bytes", machine->kernel_filename, machine->ram_size);
            exit(1);
        }
        memcpy(memory_region_get_ram_ptr(&s->ram), image, image_size);
    }

    myemulator32_write_boot_epoch();
    s->cpu = MYEMULATOR32_CPU(cpu_create(machine->cpu_type));
    myemulator32_debug_register_qmp();

    DeviceState *console = qdev_new(TYPE_MYEMULATOR32_CONSOLE);
    Chardev *console_chr = serial_hd(0);
    if (console_chr) {
        qdev_prop_set_chr(console, "chardev", console_chr);
    }
    sysbus_realize_and_unref(SYS_BUS_DEVICE(console), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(console), 0, MYEMU32_CONSOLE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(console), 0,
                       qemu_allocate_irq(myemulator32_machine_irq, s->cpu,
                                         MYEMU32_CONSOLE_IRQ));

    BlockBackend *disk = blk_by_name("myemulator2-disk");
    DeviceState *block = qdev_new(TYPE_MYEMULATOR32_BLOCK);
    if (disk) {
        qdev_prop_set_drive(block, "drive", disk);
    }
    sysbus_realize_and_unref(SYS_BUS_DEVICE(block), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(block), 0, MYEMU32_BLOCK_BASE);

    DeviceState *power = qdev_new(TYPE_MYEMULATOR32_POWER);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(power), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(power), 0, MYEMU32_POWER_BASE);

    myemulator32_create_network(s);

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
    mc->default_nic = TYPE_VIRTIO_NET;
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
