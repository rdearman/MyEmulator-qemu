#ifndef MYEMULATOR32_CPU_QOM_H
#define MYEMULATOR32_CPU_QOM_H

#include "exec/cpu-defs.h"
#include "hw/core/cpu.h"
#include "qom/object.h"

#define TYPE_MYEMULATOR32_CPU "myemulator32-cpu"
#define MYEMULATOR32_CPU_TYPE_NAME(model) TYPE_MYEMULATOR32_CPU "-" model

OBJECT_DECLARE_CPU_TYPE(MyEmulator32CPU, MyEmulator32CPUClass,
                        MYEMULATOR32_CPU)

#endif
