#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "trace.h"
#include "myemulator-timer.h"

enum {
    TIMER_CONTROL = 0,
    TIMER_STATUS = 1,
    TIMER_COUNT_LO = 2,
    TIMER_COUNT_HI = 3,
    TIMER_RELOAD_LO = 4,
    TIMER_RELOAD_HI = 5,
};

#define NS_PER_MS 1000000LL

static void myemulator_timer_update_irq(MyEmulatorTimerState *s)
{
    bool asserted = (s->control & MYEMULATOR_TIMER_IRQ_ENABLE) && s->expired;

    if (asserted == s->irq_asserted) {
        return;
    }
    qemu_set_irq(s->irq, asserted);
    s->irq_asserted = asserted;
    trace_myemulator_timer_irq(asserted);
}

static void myemulator_timer_schedule(MyEmulatorTimerState *s, int64_t now)
{
    uint64_t delay = s->reload ? s->reload : 1;
    s->deadline_ns = now + delay * NS_PER_MS;
    timer_mod_ns(s->timer, s->deadline_ns);
}

static void myemulator_timer_expire(void *opaque)
{
    MyEmulatorTimerState *s = opaque;
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    s->count = 0;
    s->expired = true;
    trace_myemulator_timer_expired(s->reload);
    if (s->control & MYEMULATOR_TIMER_PERIODIC) {
        s->count = s->reload;
        myemulator_timer_schedule(s, now);
    } else {
        s->control &= ~MYEMULATOR_TIMER_ENABLE;
        s->deadline_ns = 0;
    }
    myemulator_timer_update_irq(s);
}

static uint16_t myemulator_timer_live_count(MyEmulatorTimerState *s)
{
    int64_t remaining;

    if (!(s->control & MYEMULATOR_TIMER_ENABLE)) {
        return s->count;
    }
    remaining = s->deadline_ns - qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (remaining <= 0) {
        return 0;
    }
    remaining = (remaining + NS_PER_MS - 1) / NS_PER_MS;
    return MIN(remaining, 0xffff);
}

static uint64_t myemulator_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    MyEmulatorTimerState *s = opaque;
    uint16_t count;

    if (size != 1) {
        return 0;
    }
    switch (offset) {
    case TIMER_CONTROL:
        return s->control;
    case TIMER_STATUS:
        return (s->expired ? MYEMULATOR_TIMER_EXPIRED : 0) |
               ((s->control & MYEMULATOR_TIMER_ENABLE) &&
                !(s->expired && !(s->control & MYEMULATOR_TIMER_PERIODIC))
                    ? MYEMULATOR_TIMER_RUNNING : 0);
    case TIMER_COUNT_LO:
        count = myemulator_timer_live_count(s);
        return count & 0xff;
    case TIMER_COUNT_HI:
        count = myemulator_timer_live_count(s);
        return count >> 8;
    case TIMER_RELOAD_LO:
        return s->reload & 0xff;
    case TIMER_RELOAD_HI:
        return s->reload >> 8;
    default:
        return 0;
    }
}

static void myemulator_timer_write(void *opaque, hwaddr offset,
                                   uint64_t value, unsigned size)
{
    MyEmulatorTimerState *s = opaque;
    uint8_t old_control;

    if (size != 1) {
        return;
    }
    switch (offset) {
    case TIMER_CONTROL:
        old_control = s->control;
        if ((old_control & MYEMULATOR_TIMER_ENABLE) &&
            !(value & MYEMULATOR_TIMER_ENABLE)) {
            s->count = myemulator_timer_live_count(s);
        }
        s->control = value & (MYEMULATOR_TIMER_ENABLE |
                              MYEMULATOR_TIMER_PERIODIC |
                              MYEMULATOR_TIMER_IRQ_ENABLE);
        if ((old_control & MYEMULATOR_TIMER_ENABLE) &&
            !(s->control & MYEMULATOR_TIMER_ENABLE)) {
            timer_del(s->timer);
            s->deadline_ns = 0;
        } else if (!(old_control & MYEMULATOR_TIMER_ENABLE) &&
                   (s->control & MYEMULATOR_TIMER_ENABLE)) {
            s->count = s->reload;
            s->expired = false;
            myemulator_timer_schedule(s, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL));
            trace_myemulator_timer_start(s->reload);
        }
        myemulator_timer_update_irq(s);
        break;
    case TIMER_STATUS:
        if (value & MYEMULATOR_TIMER_EXPIRED) {
            s->expired = false;
            myemulator_timer_update_irq(s);
        }
        break;
    case TIMER_RELOAD_LO:
        s->reload = (s->reload & 0xff00) | (value & 0xff);
        break;
    case TIMER_RELOAD_HI:
        s->reload = (s->reload & 0x00ff) | ((value & 0xff) << 8);
        break;
    default:
        break;
    }
}

static const MemoryRegionOps timer_ops = {
    .read = myemulator_timer_read,
    .write = myemulator_timer_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 1, .max_access_size = 1 },
};

static void myemulator_timer_reset(DeviceState *dev)
{
    MyEmulatorTimerState *s = MYEMULATOR_TIMER(dev);

    if (s->irq_asserted) {
        qemu_set_irq(s->irq, false);
    }
    timer_del(s->timer);
    s->control = 0;
    s->expired = false;
    s->count = 0;
    s->reload = 0;
    s->deadline_ns = 0;
    s->irq_asserted = false;
}

static void myemulator_timer_init(Object *obj)
{
    MyEmulatorTimerState *s = MYEMULATOR_TIMER(obj);
    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, myemulator_timer_expire, s);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
    memory_region_init_io(&s->mmio, obj, &timer_ops, s,
                          "myemulator-timer", MYEMULATOR_TIMER_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void myemulator_timer_finalize(Object *obj)
{
    MyEmulatorTimerState *s = MYEMULATOR_TIMER(obj);
    timer_del(s->timer);
    timer_free(s->timer);
}

static void myemulator_timer_class_init(ObjectClass *klass, void *data)
{
    device_class_set_legacy_reset(DEVICE_CLASS(klass), myemulator_timer_reset);
}

static const TypeInfo timer_type_info = {
    .name = TYPE_MYEMULATOR_TIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MyEmulatorTimerState),
    .instance_init = myemulator_timer_init,
    .instance_finalize = myemulator_timer_finalize,
    .class_init = myemulator_timer_class_init,
};

static void timer_register_types(void)
{
    type_register_static(&timer_type_info);
}

type_init(timer_register_types)
