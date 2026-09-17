#ifndef MYEMULATOR_TIMER_H
#define MYEMULATOR_TIMER_H

#include "hw/sysbus.h"
#include "qemu/timer.h"

#define TYPE_MYEMULATOR_TIMER "myemulator-timer"
OBJECT_DECLARE_SIMPLE_TYPE(MyEmulatorTimerState, MYEMULATOR_TIMER)

#define MYEMULATOR_TIMER_BASE 0xf020
#define MYEMULATOR_TIMER_SIZE 0x06
#define MYEMULATOR_TIMER_IRQ 1

#define MYEMULATOR_TIMER_ENABLE 0x01
#define MYEMULATOR_TIMER_PERIODIC 0x02
#define MYEMULATOR_TIMER_IRQ_ENABLE 0x04
#define MYEMULATOR_TIMER_EXPIRED 0x01
#define MYEMULATOR_TIMER_RUNNING 0x02

struct MyEmulatorTimerState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    QEMUTimer *timer;
    qemu_irq irq;
    bool irq_asserted;
    uint8_t control;
    bool expired;
    uint16_t count;
    uint16_t reload;
    int64_t deadline_ns;
};

#endif
