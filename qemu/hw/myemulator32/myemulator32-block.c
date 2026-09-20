#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/block/block.h"
#include "hw/qdev-properties-system.h"
#include "sysemu/block-backend.h"
#include "myemulator32-block.h"

enum {
    BLOCK_COMMAND = 0x00,
    BLOCK_STATUS = 0x01,
    BLOCK_LBA = 0x02,
    BLOCK_COUNT = 0x0a,
    BLOCK_BUFFER = 0x0c,
    BLOCK_CAPACITY = 0x10,
    BLOCK_ERROR = 0x18,
};

enum {
    BLOCK_CMD_IDENTIFY = 1,
    BLOCK_CMD_READ = 2,
    BLOCK_CMD_WRITE = 3,
};

enum {
    BLOCK_ERROR_NONE = 0,
    BLOCK_ERROR_NO_MEDIA = 1,
    BLOCK_ERROR_RANGE = 2,
    BLOCK_ERROR_IO = 3,
    BLOCK_ERROR_MEMORY = 4,
    BLOCK_ERROR_READONLY = 5,
};

static void block_error(MyEmulator32BlockState *s, uint8_t error)
{
    s->status = MYEMU32_BLOCK_STATUS_ERROR;
    s->error = error;
}

static uint64_t block_capacity(MyEmulator32BlockState *s)
{
    int64_t bytes = s->blk ? blk_getlength(s->blk) : 0;
    return bytes > 0 ? (uint64_t)bytes / MYEMU32_BLOCK_SECTOR_SIZE : 0;
}

static bool block_range_ok(MyEmulator32BlockState *s)
{
    uint64_t capacity = block_capacity(s);
    if (!s->blk) {
        block_error(s, BLOCK_ERROR_NO_MEDIA);
        return false;
    }
    if (!s->count || s->lba > capacity || s->count > capacity - s->lba) {
        block_error(s, BLOCK_ERROR_RANGE);
        return false;
    }
    return true;
}

static void block_transfer(MyEmulator32BlockState *s, bool write)
{
    uint8_t sector[MYEMU32_BLOCK_SECTOR_SIZE];
    MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;
    unsigned i;
    int ret;

    s->status = MYEMU32_BLOCK_STATUS_BUSY;
    s->error = BLOCK_ERROR_NONE;
    if (!block_range_ok(s)) {
        return;
    }
    if (write && bdrv_is_read_only(blk_bs(s->blk))) {
        block_error(s, BLOCK_ERROR_READONLY);
        return;
    }
    for (i = 0; i < s->count; i++) {
        hwaddr guest = (hwaddr)s->buffer + i * MYEMU32_BLOCK_SECTOR_SIZE;
        /* QEMU's is_write flag describes the guest memory transaction, not
         * the disk direction: a disk write reads the sector from guest RAM,
         * while a disk read writes it into guest RAM. */
        if (write && address_space_rw(&address_space_memory, guest, attrs,
                                      sector, sizeof(sector), false) != MEMTX_OK) {
            block_error(s, BLOCK_ERROR_MEMORY);
            return;
        }
        if (write) {
            ret = blk_pwrite(s->blk,
                             (int64_t)(s->lba + i) * MYEMU32_BLOCK_SECTOR_SIZE,
                             sizeof(sector), sector, 0);
        } else {
            ret = blk_pread(s->blk,
                            (int64_t)(s->lba + i) * MYEMU32_BLOCK_SECTOR_SIZE,
                            sizeof(sector), sector, 0);
        }
        if (ret < 0) {
            block_error(s, BLOCK_ERROR_IO);
            return;
        }
        if (!write && address_space_rw(&address_space_memory, guest, attrs,
                                       sector, sizeof(sector), true) != MEMTX_OK) {
            block_error(s, BLOCK_ERROR_MEMORY);
            return;
        }
    }
    s->status = MYEMU32_BLOCK_STATUS_READY;
}

static uint64_t block_read(void *opaque, hwaddr offset, unsigned size)
{
    MyEmulator32BlockState *s = opaque;
    uint64_t value = 0;
    unsigned i;

    if (size != 1) {
        return 0xff;
    }
    switch (offset) {
    case BLOCK_STATUS: return s->status;
    case BLOCK_ERROR: return s->error;
    case BLOCK_CAPACITY ... BLOCK_CAPACITY + 7:
        i = offset - BLOCK_CAPACITY;
        return (block_capacity(s) >> (i * 8)) & 0xff;
    case BLOCK_LBA ... BLOCK_LBA + 7:
        i = offset - BLOCK_LBA;
        return (s->lba >> (i * 8)) & 0xff;
    case BLOCK_COUNT: return s->count & 0xff;
    case BLOCK_COUNT + 1: return s->count >> 8;
    case BLOCK_BUFFER ... BLOCK_BUFFER + 3:
        i = offset - BLOCK_BUFFER;
        return (s->buffer >> (i * 8)) & 0xff;
    default: return value;
    }
}

static void block_write(void *opaque, hwaddr offset, uint64_t value,
                        unsigned size)
{
    MyEmulator32BlockState *s = opaque;
    unsigned i;

    if (size != 1) {
        return;
    }
    switch (offset) {
    case BLOCK_COMMAND:
        if (value == BLOCK_CMD_IDENTIFY) {
            s->status = MYEMU32_BLOCK_STATUS_READY;
            s->error = BLOCK_ERROR_NONE;
        } else if (value == BLOCK_CMD_READ) {
            block_transfer(s, false);
        } else if (value == BLOCK_CMD_WRITE) {
            block_transfer(s, true);
        } else {
            block_error(s, BLOCK_ERROR_IO);
        }
        break;
    case BLOCK_LBA ... BLOCK_LBA + 7:
        i = offset - BLOCK_LBA;
        s->lba = (s->lba & ~(UINT64_C(0xff) << (i * 8))) |
                 ((value & 0xff) << (i * 8));
        break;
    case BLOCK_COUNT: s->count = (s->count & 0xff00) | (value & 0xff); break;
    case BLOCK_COUNT + 1: s->count = (s->count & 0x00ff) | ((value & 0xff) << 8); break;
    case BLOCK_BUFFER ... BLOCK_BUFFER + 3:
        i = offset - BLOCK_BUFFER;
        s->buffer = (s->buffer & ~(UINT32_C(0xff) << (i * 8))) |
                    ((value & 0xff) << (i * 8));
        break;
    default: break;
    }
}

static const MemoryRegionOps block_ops = {
    .read = block_read,
    .write = block_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 1, .max_access_size = 1 },
};

static void block_reset(DeviceState *dev)
{
    MyEmulator32BlockState *s = MYEMULATOR32_BLOCK(dev);
    s->lba = 0;
    s->buffer = 0;
    s->count = 0;
    s->error = BLOCK_ERROR_NONE;
    s->status = s->blk ? MYEMU32_BLOCK_STATUS_READY : MYEMU32_BLOCK_STATUS_ERROR;
}

static void block_realize(DeviceState *dev, Error **errp)
{
    MyEmulator32BlockState *s = MYEMULATOR32_BLOCK(dev);

    if (s->blk) {
        uint64_t perm = BLK_PERM_CONSISTENT_READ;
        if (!bdrv_is_read_only(blk_bs(s->blk))) {
            perm |= BLK_PERM_WRITE;
        }
        if (blk_set_perm(s->blk, perm,
                         BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE_UNCHANGED,
                         errp) < 0) {
            return;
        }
    }
    memory_region_init_io(&s->mmio, OBJECT(s), &block_ops, s,
                          "myemulator32-block", MYEMU32_BLOCK_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
}

static Property block_properties[] = {
    DEFINE_PROP_DRIVE("drive", MyEmulator32BlockState, blk),
    DEFINE_PROP_END_OF_LIST(),
};

static void block_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = block_realize;
    device_class_set_props(dc, block_properties);
    device_class_set_legacy_reset(dc, block_reset);
}

static const TypeInfo block_type_info = {
    .name = TYPE_MYEMULATOR32_BLOCK,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MyEmulator32BlockState),
    .class_init = block_class_init,
};

static void block_register_types(void)
{
    type_register_static(&block_type_info);
}

type_init(block_register_types)
