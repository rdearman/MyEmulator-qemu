#ifndef MYEMULATOR_FLOPPY_H
#define MYEMULATOR_FLOPPY_H

#include "hw/sysbus.h"

#define TYPE_MYEMULATOR_FLOPPY "myemulator-floppy"
OBJECT_DECLARE_SIMPLE_TYPE(MyEmulatorFloppyState, MYEMULATOR_FLOPPY)

#define MYEMULATOR_FLOPPY_BASE 0xf000
#define MYEMULATOR_FLOPPY_SIZE 0x10
#define MYEMULATOR_FLOPPY_SECTOR_SIZE 256

struct MyEmulatorFloppyState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    BlockBackend *blk;
    uint8_t buffer[MYEMULATOR_FLOPPY_SECTOR_SIZE];
    uint16_t sector;
    uint16_t data_index;
    uint8_t status;
    uint8_t error;
};

#define MYEMULATOR_FLOPPY_STATUS_BUSY  0x01
#define MYEMULATOR_FLOPPY_STATUS_READY 0x02
#define MYEMULATOR_FLOPPY_STATUS_ERROR 0x04

#endif
