#ifndef MYEMULATOR_CONSOLE_H
#define MYEMULATOR_CONSOLE_H

#include "chardev/char-fe.h"
#include "hw/sysbus.h"
#include "qemu/fifo8.h"

#define TYPE_MYEMULATOR_CONSOLE "myemulator-console"
OBJECT_DECLARE_SIMPLE_TYPE(MyEmulatorConsoleState, MYEMULATOR_CONSOLE)

#define MYEMULATOR_CONSOLE_BASE 0xf010
#define MYEMULATOR_CONSOLE_SIZE 0x10
#define MYEMULATOR_CONSOLE_IRQ 4
#define MYEMULATOR_CONSOLE_FIFO_SIZE 16

#define MYEMULATOR_CONSOLE_STATUS_RX_READY 0x01
#define MYEMULATOR_CONSOLE_STATUS_TX_READY 0x02
#define MYEMULATOR_CONSOLE_CONTROL_RX_IRQ_ENABLE 0x01
#define MYEMULATOR_CONSOLE_IRQ_STATUS_RX_PENDING 0x01

struct MyEmulatorConsoleState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    CharBackend chr;
    qemu_irq irq;
    Fifo8 rx_fifo;
    uint8_t control;
};

#endif
