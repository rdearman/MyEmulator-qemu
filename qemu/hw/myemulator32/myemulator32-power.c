#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "sysemu/runstate.h"
#include "myemulator32-power.h"

#define MYEMU32_POWER_RESTART 0x52455354u
#define MYEMU32_POWER_OFF     0x504f4646u

static void myemulator32_power_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned size)
{
    (void)opaque;
    if (offset != 0 || size != 4) {
        return;
    }
    if ((uint32_t)value == MYEMU32_POWER_RESTART) {
        qemu_system_reset(SHUTDOWN_CAUSE_GUEST_RESET);
    } else if ((uint32_t)value == MYEMU32_POWER_OFF) {
        /* Request the normal QEMU shutdown path. Do not call exit() from
         * the MMIO callback: QEMU must unwind its main loop and drain block
         * backends so guest sync() reaches the host image. */
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
    }
}

static const MemoryRegionOps myemulator32_power_ops = {
    .write = myemulator32_power_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = { .min_access_size = 4, .max_access_size = 4 },
};

static void myemulator32_power_init(Object *obj)
{
    MyEmulator32PowerState *s = MYEMULATOR32_POWER(obj);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
    memory_region_init_io(&s->mmio, obj, &myemulator32_power_ops, s,
                          "myemulator32-power", 4);
}

static const TypeInfo myemulator32_power_info = {
    .name = TYPE_MYEMULATOR32_POWER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MyEmulator32PowerState),
    .instance_init = myemulator32_power_init,
};

static void myemulator32_power_register_types(void)
{
    type_register_static(&myemulator32_power_info);
}

type_init(myemulator32_power_register_types)
