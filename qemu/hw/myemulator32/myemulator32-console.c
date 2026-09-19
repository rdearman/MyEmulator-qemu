#include "qemu/osdep.h"
#include "chardev/char-fe.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"
#include "hw/sysbus.h"
#include "qemu/fifo8.h"
#include "myemulator32-console.h"

enum {
    MYEMU32_CONSOLE_DATA = 0,
    MYEMU32_CONSOLE_STATUS = 1,
    MYEMU32_CONSOLE_CONTROL = 2,
    MYEMU32_CONSOLE_IRQ_STATUS = 3,
};

static bool myemu32_console_irq_condition(MyEmulator32ConsoleState *s)
{
    return (s->control & MYEMU32_CONSOLE_CONTROL_RX_IRQ_ENABLE) &&
           !fifo8_is_empty(&s->rx_fifo);
}

static void myemu32_console_update_irq(MyEmulator32ConsoleState *s)
{
    qemu_set_irq(s->irq, myemu32_console_irq_condition(s));
}

static int myemu32_console_can_receive(void *opaque)
{
    return fifo8_num_free(&((MyEmulator32ConsoleState *)opaque)->rx_fifo);
}

static void myemu32_console_receive(void *opaque, const uint8_t *buf, int size)
{
    MyEmulator32ConsoleState *s = opaque;
    for (int i = 0; i < size; i++) {
        if (!fifo8_is_full(&s->rx_fifo)) {
            fifo8_push(&s->rx_fifo, buf[i]);
        }
    }
    myemu32_console_update_irq(s);
}

static uint64_t myemu32_console_read(void *opaque, hwaddr offset, unsigned size)
{
    MyEmulator32ConsoleState *s = opaque;
    uint8_t value = 0;

    if (size != 1) {
        return 0;
    }
    switch (offset) {
    case MYEMU32_CONSOLE_DATA:
        if (!fifo8_is_empty(&s->rx_fifo)) {
            value = fifo8_pop(&s->rx_fifo);
            qemu_chr_fe_accept_input(&s->chr);
            myemu32_console_update_irq(s);
        }
        return value;
    case MYEMU32_CONSOLE_STATUS:
        if (!fifo8_is_empty(&s->rx_fifo)) {
            value |= MYEMU32_CONSOLE_STATUS_RX_READY;
        }
        return value | MYEMU32_CONSOLE_STATUS_TX_READY;
    case MYEMU32_CONSOLE_CONTROL:
        return s->control;
    case MYEMU32_CONSOLE_IRQ_STATUS:
        return myemu32_console_irq_condition(s) ?
               MYEMU32_CONSOLE_IRQ_STATUS_RX_PENDING : 0;
    default:
        return 0;
    }
}

static void myemu32_console_write(void *opaque, hwaddr offset,
                                  uint64_t value, unsigned size)
{
    MyEmulator32ConsoleState *s = opaque;
    uint8_t byte = value;

    if (size != 1) {
        return;
    }
    switch (offset) {
    case MYEMU32_CONSOLE_DATA:
        if (byte == '\n') {
            const uint8_t newline[] = {'\r', '\n'};
            qemu_chr_fe_write(&s->chr, newline, sizeof(newline));
        } else {
            qemu_chr_fe_write(&s->chr, &byte, 1);
        }
        break;
    case MYEMU32_CONSOLE_CONTROL:
        s->control = value & MYEMU32_CONSOLE_CONTROL_RX_IRQ_ENABLE;
        myemu32_console_update_irq(s);
        break;
    default:
        break;
    }
}

static const MemoryRegionOps myemu32_console_ops = {
    .read = myemu32_console_read,
    .write = myemu32_console_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 1, .max_access_size = 1 },
};

static void myemu32_console_reset(DeviceState *dev)
{
    MyEmulator32ConsoleState *s = MYEMULATOR32_CONSOLE(dev);
    fifo8_reset(&s->rx_fifo);
    s->control = 0;
    myemu32_console_update_irq(s);
}

static void myemu32_console_init(Object *obj)
{
    MyEmulator32ConsoleState *s = MYEMULATOR32_CONSOLE(obj);
    fifo8_create(&s->rx_fifo, MYEMU32_CONSOLE_FIFO_SIZE);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
    memory_region_init_io(&s->mmio, obj, &myemu32_console_ops, s,
                          "myemulator32-console", MYEMU32_CONSOLE_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void myemu32_console_realize(DeviceState *dev, Error **errp)
{
    MyEmulator32ConsoleState *s = MYEMULATOR32_CONSOLE(dev);
    qemu_chr_fe_set_handlers(&s->chr, myemu32_console_can_receive,
                             myemu32_console_receive, NULL, NULL, s,
                             NULL, true);
    myemu32_console_reset(dev);
}

static void myemu32_console_finalize(Object *obj)
{
    MyEmulator32ConsoleState *s = MYEMULATOR32_CONSOLE(obj);
    qemu_chr_fe_deinit(&s->chr, false);
    fifo8_destroy(&s->rx_fifo);
}

static Property myemu32_console_properties[] = {
    DEFINE_PROP_CHR("chardev", MyEmulator32ConsoleState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void myemu32_console_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = myemu32_console_realize;
    device_class_set_props(dc, myemu32_console_properties);
    device_class_set_legacy_reset(dc, myemu32_console_reset);
}

static const TypeInfo myemu32_console_type_info = {
    .name = TYPE_MYEMULATOR32_CONSOLE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MyEmulator32ConsoleState),
    .instance_init = myemu32_console_init,
    .instance_finalize = myemu32_console_finalize,
    .class_init = myemu32_console_class_init,
};

static void myemu32_console_register_types(void)
{
    type_register_static(&myemu32_console_type_info);
}

type_init(myemu32_console_register_types)
