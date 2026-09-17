#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/block/block.h"
#include "hw/qdev-properties-system.h"
#include "hw/sysbus.h"
#include "sysemu/block-backend.h"
#include "trace.h"
#include "myemulator-floppy.h"

enum {
    MYEMULATOR_FLOPPY_COMMAND = 0x00,
    MYEMULATOR_FLOPPY_STATUS = 0x01,
    MYEMULATOR_FLOPPY_SECTOR_LO = 0x02,
    MYEMULATOR_FLOPPY_SECTOR_HI = 0x03,
    MYEMULATOR_FLOPPY_DATA = 0x04,
    MYEMULATOR_FLOPPY_ERROR = 0x05,
};

enum {
    MYEMULATOR_FLOPPY_ERROR_NONE = 0,
    MYEMULATOR_FLOPPY_ERROR_NO_MEDIA = 1,
    MYEMULATOR_FLOPPY_ERROR_RANGE = 2,
    MYEMULATOR_FLOPPY_ERROR_IO = 3,
    MYEMULATOR_FLOPPY_ERROR_SHORT_WRITE = 4,
};

static void myemulator_floppy_set_error(MyEmulatorFloppyState *s,
                                        uint8_t error)
{
    s->status = MYEMULATOR_FLOPPY_STATUS_ERROR;
    s->error = error;
    trace_myemulator_floppy_error(error);
}

static bool myemulator_floppy_valid_sector(MyEmulatorFloppyState *s)
{
    int64_t length;

    if (!s->blk) {
        myemulator_floppy_set_error(s, MYEMULATOR_FLOPPY_ERROR_NO_MEDIA);
        return false;
    }

    length = blk_getlength(s->blk);
    if (length < 0 || (uint64_t)s->sector >=
                      (uint64_t)length / MYEMULATOR_FLOPPY_SECTOR_SIZE) {
        myemulator_floppy_set_error(s, MYEMULATOR_FLOPPY_ERROR_RANGE);
        return false;
    }
    return true;
}

static void myemulator_floppy_read_sector(MyEmulatorFloppyState *s)
{
    int ret;

    s->status = MYEMULATOR_FLOPPY_STATUS_BUSY;
    s->error = MYEMULATOR_FLOPPY_ERROR_NONE;
    if (!myemulator_floppy_valid_sector(s)) {
        return;
    }

    ret = blk_pread(s->blk,
                    (int64_t)s->sector * MYEMULATOR_FLOPPY_SECTOR_SIZE,
                    MYEMULATOR_FLOPPY_SECTOR_SIZE, s->buffer, 0);
    if (ret < 0) {
        myemulator_floppy_set_error(s, MYEMULATOR_FLOPPY_ERROR_IO);
        return;
    }

    s->data_index = 0;
    s->status = MYEMULATOR_FLOPPY_STATUS_READY;
    trace_myemulator_floppy_read(s->sector);
}

static void myemulator_floppy_write_sector(MyEmulatorFloppyState *s)
{
    int ret;

    s->status = MYEMULATOR_FLOPPY_STATUS_BUSY;
    s->error = MYEMULATOR_FLOPPY_ERROR_NONE;
    if (s->data_index != MYEMULATOR_FLOPPY_SECTOR_SIZE) {
        myemulator_floppy_set_error(s, MYEMULATOR_FLOPPY_ERROR_SHORT_WRITE);
        return;
    }
    if (!myemulator_floppy_valid_sector(s)) {
        return;
    }

    ret = blk_pwrite(s->blk,
                     (int64_t)s->sector * MYEMULATOR_FLOPPY_SECTOR_SIZE,
                     MYEMULATOR_FLOPPY_SECTOR_SIZE, s->buffer, 0);
    if (ret < 0) {
        myemulator_floppy_set_error(s, MYEMULATOR_FLOPPY_ERROR_IO);
        return;
    }

    s->status = MYEMULATOR_FLOPPY_STATUS_READY;
    trace_myemulator_floppy_write(s->sector);
}

static uint64_t myemulator_floppy_read(void *opaque, hwaddr offset,
                                       unsigned size)
{
    MyEmulatorFloppyState *s = opaque;

    if (size != 1) {
        return 0xff;
    }

    switch (offset) {
    case MYEMULATOR_FLOPPY_STATUS:
        return s->status;
    case MYEMULATOR_FLOPPY_SECTOR_LO:
        return s->sector & 0xff;
    case MYEMULATOR_FLOPPY_SECTOR_HI:
        return s->sector >> 8;
    case MYEMULATOR_FLOPPY_DATA:
        if (s->data_index >= MYEMULATOR_FLOPPY_SECTOR_SIZE) {
            s->data_index = 0;
        }
        return s->buffer[s->data_index++];
    case MYEMULATOR_FLOPPY_ERROR:
        return s->error;
    default:
        return 0xff;
    }
}

static void myemulator_floppy_write(void *opaque, hwaddr offset,
                                    uint64_t value, unsigned size)
{
    MyEmulatorFloppyState *s = opaque;

    if (size != 1) {
        return;
    }

    switch (offset) {
    case MYEMULATOR_FLOPPY_COMMAND:
        trace_myemulator_floppy_command(value, s->sector);
        if (value == 0x01) {
            myemulator_floppy_read_sector(s);
        } else if (value == 0x02) {
            myemulator_floppy_write_sector(s);
        } else {
            myemulator_floppy_set_error(s, MYEMULATOR_FLOPPY_ERROR_IO);
        }
        break;
    case MYEMULATOR_FLOPPY_SECTOR_LO:
        s->sector = (s->sector & 0xff00) | (value & 0xff);
        s->data_index = 0;
        s->status = 0;
        s->error = MYEMULATOR_FLOPPY_ERROR_NONE;
        break;
    case MYEMULATOR_FLOPPY_SECTOR_HI:
        s->sector = (s->sector & 0x00ff) | ((value & 0xff) << 8);
        s->data_index = 0;
        s->status = 0;
        s->error = MYEMULATOR_FLOPPY_ERROR_NONE;
        break;
    case MYEMULATOR_FLOPPY_DATA:
        if (s->data_index < MYEMULATOR_FLOPPY_SECTOR_SIZE) {
            s->buffer[s->data_index++] = value;
        }
        break;
    default:
        break;
    }
}

static const MemoryRegionOps myemulator_floppy_ops = {
    .read = myemulator_floppy_read,
    .write = myemulator_floppy_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void myemulator_floppy_reset(DeviceState *dev)
{
    MyEmulatorFloppyState *s = MYEMULATOR_FLOPPY(dev);

    memset(s->buffer, 0, sizeof(s->buffer));
    s->sector = 0;
    s->data_index = 0;
    s->status = 0;
    s->error = MYEMULATOR_FLOPPY_ERROR_NONE;
}

static void myemulator_floppy_realize(DeviceState *dev, Error **errp)
{
    MyEmulatorFloppyState *s = MYEMULATOR_FLOPPY(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &myemulator_floppy_ops, s,
                          "myemulator-floppy", MYEMULATOR_FLOPPY_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
}

static Property myemulator_floppy_properties[] = {
    DEFINE_PROP_DRIVE("drive", MyEmulatorFloppyState, blk),
    DEFINE_PROP_END_OF_LIST(),
};

static void myemulator_floppy_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = myemulator_floppy_realize;
    device_class_set_props(dc, myemulator_floppy_properties);
    device_class_set_legacy_reset(dc, myemulator_floppy_reset);
}

static const TypeInfo myemulator_floppy_type_info = {
    .name = TYPE_MYEMULATOR_FLOPPY,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MyEmulatorFloppyState),
    .class_init = myemulator_floppy_class_init,
};

static void myemulator_floppy_register_types(void)
{
    type_register_static(&myemulator_floppy_type_info);
}

type_init(myemulator_floppy_register_types)
