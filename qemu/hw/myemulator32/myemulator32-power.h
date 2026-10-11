#ifndef MYEMULATOR32_POWER_H
#define MYEMULATOR32_POWER_H

#include "hw/sysbus.h"

#define TYPE_MYEMULATOR32_POWER "myemulator32-power"
OBJECT_DECLARE_SIMPLE_TYPE(MyEmulator32PowerState, MYEMULATOR32_POWER)

#define MYEMU32_POWER_BASE 0xf0300000u

struct MyEmulator32PowerState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
};

#endif
