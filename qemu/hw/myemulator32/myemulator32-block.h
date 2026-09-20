#ifndef MYEMULATOR32_BLOCK_H
#define MYEMULATOR32_BLOCK_H

#include "hw/sysbus.h"

#define TYPE_MYEMULATOR32_BLOCK "myemulator32-block"
OBJECT_DECLARE_SIMPLE_TYPE(MyEmulator32BlockState, MYEMULATOR32_BLOCK)

#define MYEMU32_BLOCK_BASE 0xf0100000u
#define MYEMU32_BLOCK_SIZE 0x100u
#define MYEMU32_BLOCK_SECTOR_SIZE 512u

#define MYEMU32_BLOCK_STATUS_READY 0x01u
#define MYEMU32_BLOCK_STATUS_BUSY  0x02u
#define MYEMU32_BLOCK_STATUS_ERROR 0x04u

struct MyEmulator32BlockState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    BlockBackend *blk;
    uint64_t lba;
    uint32_t buffer;
    uint16_t count;
    uint8_t status;
    uint8_t error;
};

#endif
