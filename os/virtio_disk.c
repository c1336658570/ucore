// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
// qemu presents a "legacy" virtio interface.
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device
// virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//qemu的virtio磁盘设备驱动程序。
//使用qemu的mmio接口连接到virtio。
//qemu 提供了一个“传统”的 virtio 接口。
//qemu ...-drive file=fs.img,if=none,format=raw,id=x0 -device
//virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

//新增，用来处理磁盘中断

#include "bio.h"
#include "defs.h"
#include "file.h"
#include "fs.h"
#include "plic.h"
#include "riscv.h"
#include "types.h"
#include "virtio.h"

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct disk {
	// the virtio driver and device mostly communicate through a set of
	// structures in RAM. pages[] allocates that memory. pages[] is a
	// global (instead of calls to kalloc()) because it must consist of
	// two contiguous pages of page-aligned physical memory.
	char pages[2 * PGSIZE];

	// pages[] is divided into three regions (descriptors, avail, and
	// used), as explained in Section 2.6 of the virtio specification
	// for the legacy interface.
	// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf

	// the first region of pages[] is a set (not a ring) of DMA
	// descriptors, with which the driver tells the device where to read
	// and write individual disk operations. there are NUM descriptors.
	// most commands consist of a "chain" (a linked list) of a couple of
	// these descriptors.
	// points into pages[].
	struct virtq_desc *desc;

	// next is a ring in which the driver writes descriptor numbers
	// that the driver would like the device to process.  it only
	// includes the head descriptor of each chain. the ring has
	// NUM elements.
	// points into pages[].
	struct virtq_avail *avail;

	// finally a ring in which the device writes descriptor numbers that
	// the device has finished processing (just the head of each chain).
	// there are NUM used ring entries.
	// points into pages[].
	struct virtq_used *used;

	// our own book-keeping.
	char free[NUM]; // is a descriptor free?
	uint16 used_idx; // we've looked this far in used[2..NUM].

	// track info about in-flight operations,
	// for use when completion interrupt arrives.
	// indexed by first descriptor index of chain.
	struct {
		struct buf *b;
		char status;
	} info[NUM];

	// disk command headers.
	// one-for-one with descriptors, for convenience.
	struct virtio_blk_req ops[NUM];
} __attribute__((aligned(PGSIZE))) disk;

//完成磁盘设备的初始化和对其管理的初始化。
void virtio_disk_init()
{
	uint32 status = 0;

	if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
	    *R(VIRTIO_MMIO_VERSION) != 1 || *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
	    *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
		panic("could not find virtio disk");
	}

	status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
	*R(VIRTIO_MMIO_STATUS) = status;

	status |= VIRTIO_CONFIG_S_DRIVER;
	*R(VIRTIO_MMIO_STATUS) = status;

	// negotiate features
	uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
	features &= ~(1 << VIRTIO_BLK_F_RO);
	features &= ~(1 << VIRTIO_BLK_F_SCSI);
	features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
	features &= ~(1 << VIRTIO_BLK_F_MQ);
	features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
	features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
	features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
	*R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

	// tell device that feature negotiation is complete.
	status |= VIRTIO_CONFIG_S_FEATURES_OK;
	*R(VIRTIO_MMIO_STATUS) = status;

	// tell device we're completely ready.
	status |= VIRTIO_CONFIG_S_DRIVER_OK;
	*R(VIRTIO_MMIO_STATUS) = status;

	*R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

	// initialize queue 0.
	*R(VIRTIO_MMIO_QUEUE_SEL) = 0;
	uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (max == 0)
		panic("virtio disk has no queue 0");
	if (max < NUM)
		panic("virtio disk max queue too short");
	*R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
	memset(disk.pages, 0, sizeof(disk.pages));
	*R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.pages) >> PGSHIFT;

	// desc = pages -- num * virtq_desc
	// avail = pages + 0x40 -- 2 * uint16, then num * uint16
	// used = pages + 4096 -- 2 * uint16, then num * vRingUsedElem

	disk.desc = (struct virtq_desc *)disk.pages;
	disk.avail = (struct virtq_avail *)(disk.pages +
					    NUM * sizeof(struct virtq_desc));
	disk.used = (struct virtq_used *)(disk.pages + PGSIZE);

	// all NUM descriptors start out unused.
	for (int i = 0; i < NUM; i++)
		disk.free[i] = 1;

	// plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
//找到一个空闲描述符，将其标记为非空闲，返回其索引。
static int alloc_desc()
{
	for (int i = 0; i < NUM; i++) {
		if (disk.free[i]) {
			disk.free[i] = 0;
			return i;
		}
	}
	return -1;
}

// mark a descriptor as free.
//将描述符标记为空闲。
static void free_desc(int i)
{
	if (i >= NUM)
		panic("free_desc 1");
	if (disk.free[i])
		panic("free_desc 2");
	disk.desc[i].addr = 0;
	disk.desc[i].len = 0;
	disk.desc[i].flags = 0;
	disk.desc[i].next = 0;
	disk.free[i] = 1;
}

// free a chain of descriptors.
//释放一个描述符链。
static void free_chain(int i)
{
	while (1) {
		int flag = disk.desc[i].flags;
		int nxt = disk.desc[i].next;
		free_desc(i);
		if (flag & VRING_DESC_F_NEXT)
			i = nxt;
		else
			break;
	}
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
//分配三个描述符（它们不需要连续）。
//磁盘传输总是使用三个描述符。
static int alloc3_desc(int *idx)
{
	for (int i = 0; i < 3; i++) {
		idx[i] = alloc_desc();
		if (idx[i] < 0) {
			for (int j = 0; j < i; j++)
				free_desc(idx[j]);
			return -1;
		}
	}
	return 0;
}

extern int PID;


//实际完成磁盘IO，当设定好读写信息后会通过MMIO的方式通知磁盘开始写。
//然后，os会开启中断并开始死等磁盘读写完成。当磁盘完成 IO 后，磁盘会触发一个外部中断，
//在中断处理中会把死循环条件解除。内核态只会在处理磁盘读写的时候短暂开启中断，之后会马上关闭。
void virtio_disk_rw(struct buf *b, int write)
{
	uint64 sector = b->blockno * (BSIZE / 512);
	// the spec's Section 5.2 says that legacy block operations use
	// three descriptors: one for type/reserved/sector, one for the
	// data, one for a 1-byte status result.
	// allocate the three descriptors.
	int idx[3];
	while (1) {
		if (alloc3_desc(idx) == 0) {
			break;
		}
		yield();
	}
	// format the three descriptors.
	// qemu's virtio-blk.c reads them.
	struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

	if (write)
		buf0->type = VIRTIO_BLK_T_OUT; // write the disk
	else
		buf0->type = VIRTIO_BLK_T_IN; // read the disk
	buf0->reserved = 0;
	buf0->sector = sector;

	disk.desc[idx[0]].addr = (uint64)buf0;
	disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
	disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
	disk.desc[idx[0]].next = idx[1];

	disk.desc[idx[1]].addr = (uint64)b->data;
	disk.desc[idx[1]].len = BSIZE;
	if (write)
		disk.desc[idx[1]].flags = 0; // device reads b->data
	else
		disk.desc[idx[1]].flags =
			VRING_DESC_F_WRITE; // device writes b->data
	disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
	disk.desc[idx[1]].next = idx[2];

	disk.info[idx[0]].status = 0xfb; // device writes 0 on success
	disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
	disk.desc[idx[2]].len = 1;
	disk.desc[idx[2]].flags =
		VRING_DESC_F_WRITE; // device writes the status
	disk.desc[idx[2]].next = 0;

	// record struct buf for virtio_disk_intr().
	b->disk = 1;
	disk.info[idx[0]].b = b;

	// tell the device the first index in our chain of descriptors.
	disk.avail->ring[disk.avail->idx % NUM] = idx[0];

	__sync_synchronize();

	// tell the device another avail ring entry is available.
	disk.avail->idx += 1; // not % NUM ...

	__sync_synchronize();

	*R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

	// Wait for virtio_disk_intr() to say request has finished.
	// Make sure complier will load 'b' form memory
	struct buf volatile *_b = b;
	//开内核中断
	intr_on();
	//在这里循环死等，直到处发内核中断，然后在内核中断中修改_b->disk，代表磁盘IO完成，然后退出循环，继续向下执行
	while (_b->disk == 1) {		
		// WARN: No kernel concurrent support, DO NOT allow kernel yield
		// yield();
	}
	//关内核中断
	intr_off();
	disk.info[idx[0]].b = 0;
	free_chain(idx[0]);
}

void virtio_disk_intr()
{
	// the device won't raise another interrupt until we tell it
	// we've seen this interrupt, which the following line does.
	// this may race with the device writing new entries to
	// the "used" ring, in which case we may process the new
	// completion entries in this interrupt, and have nothing to do
	// in the next interrupt, which is harmless.
	*R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

	__sync_synchronize();

	// the device increments disk.used->idx when it
	// adds an entry to the used ring.

	while (disk.used_idx != disk.used->idx) {
		__sync_synchronize();
		int id = disk.used->ring[disk.used_idx % NUM].id;

		if (disk.info[id].status != 0)
			panic("virtio_disk_intr status");

		struct buf *b = disk.info[id].b;
		b->disk = 0; // disk is done with buf	将b->disk置0
		disk.used_idx += 1;
	}
}