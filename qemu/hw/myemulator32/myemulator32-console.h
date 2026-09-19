#ifndef MYEMULATOR32_CONSOLE_H
#define MYEMULATOR32_CONSOLE_H

#include "chardev/char-fe.h"
#include "hw/sysbus.h"
#include "qemu/fifo8.h"

#define TYPE_MYEMULATOR32_CONSOLE "myemulator32-console"
OBJECT_DECLARE_SIMPLE_TYPE(MyEmulator32ConsoleState, MYEMULATOR32_CONSOLE)

#define MYEMU32_CONSOLE_BASE 0xf0000000u
#define MYEMU32_CONSOLE_SIZE 0x10u
#define MYEMU32_CONSOLE_IRQ 4u
#define MYEMU32_CONSOLE_FIFO_SIZE 64u

#define MYEMU32_CONSOLE_STATUS_RX_READY 0x01u
#define MYEMU32_CONSOLE_STATUS_TX_READY 0x02u
#define MYEMU32_CONSOLE_CONTROL_RX_IRQ_ENABLE 0x01u
#define MYEMU32_CONSOLE_IRQ_STATUS_RX_PENDING 0x01u

struct MyEmulator32ConsoleState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    CharBackend chr;
    qemu_irq irq;
    Fifo8 rx_fifo;
    uint8_t control;
};

#endif
