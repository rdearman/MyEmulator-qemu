#include "qemu/osdep.h"
#include "chardev/char-fe.h"
#include "hw/irq.h"
#include "hw/qdev-properties-system.h"
#include "hw/sysbus.h"
#include "qemu/fifo8.h"
#include "trace.h"
#include "myemulator-console.h"

enum {
    MYEMULATOR_CONSOLE_DATA = 0x00,
    MYEMULATOR_CONSOLE_STATUS = 0x01,
    MYEMULATOR_CONSOLE_CONTROL = 0x02,
    MYEMULATOR_CONSOLE_IRQ_STATUS = 0x03,
};

static bool myemulator_console_irq_condition(MyEmulatorConsoleState *s)
{
    return (s->control & MYEMULATOR_CONSOLE_CONTROL_RX_IRQ_ENABLE) != 0 &&
           !fifo8_is_empty(&s->rx_fifo);
}

static void myemulator_console_update_irq(MyEmulatorConsoleState *s)
{
    bool asserted = myemulator_console_irq_condition(s);

    qemu_set_irq(s->irq, asserted);
    trace_myemulator_console_irq(asserted);
}

static int myemulator_console_can_receive(void *opaque)
{
    MyEmulatorConsoleState *s = opaque;

    return fifo8_num_free(&s->rx_fifo);
}

static void myemulator_console_receive(void *opaque, const uint8_t *buf,
                                       int size)
{
    MyEmulatorConsoleState *s = opaque;
    int i;

    for (i = 0; i < size; i++) {
        if (fifo8_is_full(&s->rx_fifo)) {
            trace_myemulator_console_overflow(buf[i]);
            continue;
        }
        fifo8_push(&s->rx_fifo, buf[i]);
        trace_myemulator_console_received(buf[i]);
    }
    myemulator_console_update_irq(s);
}

static uint64_t myemulator_console_read(void *opaque, hwaddr offset,
                                        unsigned size)
{
    MyEmulatorConsoleState *s = opaque;
    uint8_t value = 0;

    if (size != 1) {
        return 0;
    }

    switch (offset) {
    case MYEMULATOR_CONSOLE_DATA:
        if (!fifo8_is_empty(&s->rx_fifo)) {
            value = fifo8_pop(&s->rx_fifo);
            qemu_chr_fe_accept_input(&s->chr);
            myemulator_console_update_irq(s);
        }
        return value;
    case MYEMULATOR_CONSOLE_STATUS:
        if (!fifo8_is_empty(&s->rx_fifo)) {
            value |= MYEMULATOR_CONSOLE_STATUS_RX_READY;
        }
        if (qemu_chr_fe_backend_connected(&s->chr)) {
            value |= MYEMULATOR_CONSOLE_STATUS_TX_READY;
        } else {
            /* Output is discarded when no backend is attached. */
            value |= MYEMULATOR_CONSOLE_STATUS_TX_READY;
        }
        return value;
    case MYEMULATOR_CONSOLE_CONTROL:
        return s->control;
    case MYEMULATOR_CONSOLE_IRQ_STATUS:
        return myemulator_console_irq_condition(s) ?
               MYEMULATOR_CONSOLE_IRQ_STATUS_RX_PENDING : 0;
    default:
        return 0;
    }
}

static void myemulator_console_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned size)
{
    MyEmulatorConsoleState *s = opaque;
    uint8_t byte = value;

    if (size != 1) {
        return;
    }

    switch (offset) {
    case MYEMULATOR_CONSOLE_DATA:
        /* Guest software uses Unix LF.  Translate only at the host chardev
         * boundary so terminal backends perform a complete line return.
         * Guest-visible MMIO remains raw bytes. */
        if (byte == '\n') {
            const uint8_t newline[] = {'\r', '\n'};
            qemu_chr_fe_write(&s->chr, newline, sizeof(newline));
        } else {
            qemu_chr_fe_write(&s->chr, &byte, 1);
        }
        trace_myemulator_console_transmitted(byte);
        break;
    case MYEMULATOR_CONSOLE_CONTROL:
        s->control = value & MYEMULATOR_CONSOLE_CONTROL_RX_IRQ_ENABLE;
        myemulator_console_update_irq(s);
        break;
    default:
        /* IRQ_STATUS is level-derived and is acknowledged by DATA reads. */
        break;
    }
}

static const MemoryRegionOps myemulator_console_ops = {
    .read = myemulator_console_read,
    .write = myemulator_console_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void myemulator_console_reset(DeviceState *dev)
{
    MyEmulatorConsoleState *s = MYEMULATOR_CONSOLE(dev);

    fifo8_reset(&s->rx_fifo);
    s->control = 0;
    myemulator_console_update_irq(s);
}

static void myemulator_console_init(Object *obj)
{
    MyEmulatorConsoleState *s = MYEMULATOR_CONSOLE(obj);

    fifo8_create(&s->rx_fifo, MYEMULATOR_CONSOLE_FIFO_SIZE);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
    memory_region_init_io(&s->mmio, obj, &myemulator_console_ops, s,
                          "myemulator-console", MYEMULATOR_CONSOLE_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void myemulator_console_realize(DeviceState *dev, Error **errp)
{
    MyEmulatorConsoleState *s = MYEMULATOR_CONSOLE(dev);

    qemu_chr_fe_set_handlers(&s->chr, myemulator_console_can_receive,
                             myemulator_console_receive, NULL, NULL, s,
                             NULL, true);
    myemulator_console_reset(dev);
}

static void myemulator_console_finalize(Object *obj)
{
    MyEmulatorConsoleState *s = MYEMULATOR_CONSOLE(obj);

    qemu_chr_fe_deinit(&s->chr, false);
    fifo8_destroy(&s->rx_fifo);
}

static Property myemulator_console_properties[] = {
    DEFINE_PROP_CHR("chardev", MyEmulatorConsoleState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void myemulator_console_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = myemulator_console_realize;
    device_class_set_props(dc, myemulator_console_properties);
    device_class_set_legacy_reset(dc, myemulator_console_reset);
}

static const TypeInfo myemulator_console_type_info = {
    .name = TYPE_MYEMULATOR_CONSOLE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MyEmulatorConsoleState),
    .instance_init = myemulator_console_init,
    .instance_finalize = myemulator_console_finalize,
    .class_init = myemulator_console_class_init,
};

static void myemulator_console_register_types(void)
{
    type_register_static(&myemulator_console_type_info);
}

type_init(myemulator_console_register_types)
