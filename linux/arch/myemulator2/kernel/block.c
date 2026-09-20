// SPDX-License-Identifier: GPL-2.0
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>

#define MYEMU2_BLOCK_BASE 0xf0100000UL
#define MYEMU2_BLOCK_SIZE 0x100
#define MYEMU2_SECTOR_SIZE 512
#define MYEMU2_STATUS_READY 0x01
#define MYEMU2_STATUS_BUSY 0x02
#define MYEMU2_STATUS_ERROR 0x04
#define MYEMU2_CMD_IDENTIFY 1
#define MYEMU2_CMD_READ 2
#define MYEMU2_CMD_WRITE 3

#define REG_COMMAND 0x00
#define REG_STATUS 0x01
#define REG_LBA 0x02
#define REG_COUNT 0x0a
#define REG_BUFFER 0x0c
#define REG_CAPACITY 0x10
#define REG_ERROR 0x18

static void __iomem *myemu2_block;
static struct blk_mq_tag_set myemu2_tag_set;
static struct gendisk *myemu2_disk;

static const struct block_device_operations myemu2_fops = {
	.owner = THIS_MODULE,
};

static void block_reg32(unsigned int reg, u32 value)
{
	unsigned int i;
	for (i = 0; i < 4; i++)
		writeb(value >> (i * 8), myemu2_block + reg + i);
}

static void block_reg64(unsigned int reg, u64 value)
{
	unsigned int i;
	for (i = 0; i < 8; i++)
		writeb(value >> (i * 8), myemu2_block + reg + i);
}

static u64 block_read64(unsigned int reg)
{
	u64 value = 0;
	unsigned int i;
	for (i = 0; i < 8; i++)
		value |= (u64)readb(myemu2_block + reg + i) << (i * 8);
	return value;
}

static blk_status_t myemu2_transfer(struct request *rq, bool write)
{
	struct bio_vec bvec;
	struct req_iterator iter;
	sector_t sector = blk_rq_pos(rq);

	rq_for_each_segment(bvec, rq, iter) {
		void *addr = kmap_local_page(bvec.bv_page) + bvec.bv_offset;
		unsigned int bytes = bvec.bv_len;
		unsigned int count;

		if (!bytes || bytes % MYEMU2_SECTOR_SIZE ||
			!IS_ALIGNED((unsigned long)addr, MYEMU2_SECTOR_SIZE)) {
			kunmap_local(addr);
			return BLK_STS_IOERR;
		}
		count = bytes / MYEMU2_SECTOR_SIZE;
		block_reg64(REG_LBA, sector);
		writeb(count, myemu2_block + REG_COUNT);
		writeb(0, myemu2_block + REG_COUNT + 1);
		block_reg32(REG_BUFFER, virt_to_phys(addr));
		writeb(write ? MYEMU2_CMD_WRITE : MYEMU2_CMD_READ,
		       myemu2_block + REG_COMMAND);
		while (readb(myemu2_block + REG_STATUS) == MYEMU2_STATUS_BUSY)
			cpu_relax();
		if (readb(myemu2_block + REG_STATUS) != MYEMU2_STATUS_READY) {
			kunmap_local(addr);
			return BLK_STS_IOERR;
		}
		kunmap_local(addr);
		sector += count;
	}
	return BLK_STS_OK;
}

static blk_status_t myemu2_queue_rq(struct blk_mq_hw_ctx *hctx,
					const struct blk_mq_queue_data *bd)
{
	struct request *rq = bd->rq;
	blk_status_t status;

	blk_mq_start_request(rq);
	status = myemu2_transfer(rq, rq_data_dir(rq) == WRITE);
	blk_mq_end_request(rq, status);
	return BLK_STS_OK;
}

static const struct blk_mq_ops myemu2_mq_ops = {
	.queue_rq = myemu2_queue_rq,
};

static int __init myemu2_block_init(void)
{
	struct queue_limits limits = { };
	int ret;
	u64 capacity;

	myemu2_block = ioremap(MYEMU2_BLOCK_BASE, MYEMU2_BLOCK_SIZE);
	if (!myemu2_block)
		return -ENOMEM;
	writeb(MYEMU2_CMD_IDENTIFY, myemu2_block + REG_COMMAND);
	if (readb(myemu2_block + REG_STATUS) != MYEMU2_STATUS_READY)
		return -ENODEV;
	capacity = block_read64(REG_CAPACITY);
	if (!capacity)
		return -ENODEV;

	myemu2_tag_set.ops = &myemu2_mq_ops;
	myemu2_tag_set.nr_hw_queues = 1;
	myemu2_tag_set.queue_depth = 8;
	myemu2_tag_set.numa_node = NUMA_NO_NODE;
	myemu2_tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	ret = blk_mq_alloc_tag_set(&myemu2_tag_set);
	if (ret)
		return ret;
	limits.logical_block_size = MYEMU2_SECTOR_SIZE;
	limits.physical_block_size = MYEMU2_SECTOR_SIZE;
	myemu2_disk = blk_mq_alloc_disk(&myemu2_tag_set, &limits, NULL);
	if (IS_ERR(myemu2_disk)) {
		ret = PTR_ERR(myemu2_disk);
		blk_mq_free_tag_set(&myemu2_tag_set);
		return ret;
	}
	myemu2_disk->fops = &myemu2_fops;
	strscpy(myemu2_disk->disk_name, "myemu0", DISK_NAME_LEN);
	set_capacity(myemu2_disk, capacity);
	add_disk(myemu2_disk);
	pr_info("myemulator2-block: %llu sectors dev=%u:%u\n",
		(unsigned long long)capacity, MAJOR(disk_devt(myemu2_disk)),
		MINOR(disk_devt(myemu2_disk)));
	return 0;
}
device_initcall(myemu2_block_init);
