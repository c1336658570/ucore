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
//virtio内存映射IO寄存器r的地址，其中mmio是Memory Mapped IO的缩写。
/*
在计算机系统中，IO（Input/Output，输入与输出）是指CPU与外部设备之间的数据传输。通常情况下，
CPU 通过读写内存中的特定地址来与外部设备进行通信，从而实现输入输出的功能。内存映射IO是一种广泛应用的IO技术，
它将特定的硬件寄存器映射到内存地址空间中的一个地址，通过读写这个地址即可访问设备寄存器，从而实现输入输出的功能。
virtio是一种虚拟化设备接口，其主要用于在虚拟机中模拟驱动程序，从而实现虚拟机与宿主机之间的数据传输。
在virtio中，内存映射IO也被广泛使用，通过操作virtio内存映射IO寄存器，实现虚拟机和宿主机之间的数据传输和通信。
因此，"the address of virtio mmio register r" 就表示 virtio内存映射IO寄存器r的内存地址，
可以通过读写该地址来访问virtio设备的寄存器，实现数据输入输出和通信。
*/
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

//定义了一个名为 disk 的结构体，用于在内存中分配并管理 virtio 块设备的传输结构，以便在虚拟机和物理主机之间传输数据。
static struct disk {
	// the virtio driver and device mostly communicate through a set of
	// structures in RAM. pages[] allocates that memory. pages[] is a
	// global (instead of calls to kalloc()) because it must consist of
	// two contiguous pages of page-aligned physical memory.
	
	/*
	一个长度为 2 * PGSIZE 的字符数组，用于分配 VIRTIO 设备和驱动程序之间通信所需的内存。
	具体来说，该数组被划分为三个部分，包括描述符区域，可用区域和已用区域，用于存储 DMA 描述符、
	可用描述符和已用描述符等信息。
	*/
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

	/*
	指向 DMA 描述符数组的指针。DMA 描述符用于告诉 VIRTIO 设备在何处读取或写入磁盘数据，
	其中每个操作都由一对描述符构成的链表组成。
	*/
	struct virtq_desc *desc;

	// next is a ring in which the driver writes descriptor numbers
	// that the driver would like the device to process.  it only
	// includes the head descriptor of each chain. the ring has
	// NUM elements.
	// points into pages[].

	/*
	指向可用描述符环的指针。可用描述符环是一个环状结构，用于存储驱动程序写入的描述符编号，
	表示它希望 VIRTIO 设备来处理。
	*/
	struct virtq_avail *avail;

	// finally a ring in which the device writes descriptor numbers that
	// the device has finished processing (just the head of each chain).
	// there are NUM used ring entries.
	// points into pages[].

	/*
	指向已用描述符环的指针。已用描述符环也是一个环状结构，用于存储 VIRTIO 设备已处理完成的描述符编号。
	*/
	struct virtq_used *used;

	// our own book-keeping.

	/*
	一个长度为 NUM 的字符数组，用于跟踪描述符是否已被使用。描述符没有被使用时，相应的 free 数组元素为 1，否则为 0。
	*/
	char free[NUM]; // is a descriptor free?
	/*
	一个表示已寻找到的最大已用描述符编号的 uint16 型整数。
	*/
	uint16 used_idx; // we've looked this far in used[2..NUM].

	// track info about in-flight operations,
	// for use when completion interrupt arrives.
	// indexed by first descriptor index of chain.

	/*
	一个长度为 NUM 的结构体数组，用于跟踪传输任务的信息，如任务是否完成、任务的状态以及磁盘数据的缓存地址等。
	*/
	struct {
		struct buf *b;
		char status;
	} info[NUM];

	// disk command headers.
	// one-for-one with descriptors, for convenience.

	/*
	用于保存磁盘操作的请求头。每个磁盘请求都对应一个 ops 数组元素，以便于描述符的处理。
	*/
	struct virtio_blk_req ops[NUM];
} __attribute__((aligned(PGSIZE))) disk;	//整个 disk 结构体的内存空间遵循 4KB 的对齐要求。

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

/*
用于处理 VIRTIO 块设备的中断请求。主要包括以下步骤：
告诉 VIRTIO 块设备控制器已经成功处理本次中断请求，并清除中断状态。
检查 "used" 环是否被更新，并遍历 "used" 环。当处理完成一个缓冲区后，将该缓冲区的 disk 标志位清零，
然后将指向 "used" 环的下标加1，指向下一个待处理的完成请求。
*/
void virtio_disk_intr()
{
	// the device won't raise another interrupt until we tell it
	// we've seen this interrupt, which the following line does.
	// this may race with the device writing new entries to
	// the "used" ring, in which case we may process the new
	// completion entries in this interrupt, and have nothing to do
	// in the next interrupt, which is harmless.
	//设备在产生下一个中断之前不会再次引发中断，直到我们告诉设备我们已经看到了这个中断（通过接下来的代码）。
	//这可能会和设备写入新条目到 "used" 环的过程产生竞争，这种情况下，我们可能在这个中断中处理新的完成条目，
	//接下来的中断可能没有任务需要处理，这是无害的。
	/*
	这段注释是 VIRTIO 块设备中断处理函数 virtio_disk_intr() 中的一段注释，
	//用来说明环形队列与中断处理之间可能出现的竞争条件和系统行为。
	这也提示开发者在系统设计和编程时需要注意这些可能的竞态情况，以避免引起不可预期的错误和行为。
	*/
//VIRTIO_MMIO_INTERRUPT_ACK 和 VIRTIO_MMIO_INTERRUPT_STATUS：宏，表示 VIRTIO
//设备的中断应答和状态寄存器的地址。这里的内容是告诉 VIRTIO 控制器已经成功处理了本次中断请求，并清除了中断状态。
	*R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

	//用于确保内存屏障，以避免由于乱序执行的结果而产生不良影响。
	__sync_synchronize();

	// the device increments disk.used->idx when it
	// adds an entry to the used ring.
	//设备增加 disk.used->idx 时
	//向已用环添加一个条目。
//disk.used 和 disk.used_idx：分别表示指向 "used" 环缓冲区的指针和缓冲区中待处理完成请求的下标。
//（操作系统通过 "used" 环来通知 VIRTIO 块设备已经完成了指定的读写操作。）
//disk.used->idx："used" 环缓冲区中下一个要处理的请求的下标。
//disk.used_idx != disk.used->idx：判断 "used" 环是否被修改过。
	while (disk.used_idx != disk.used->idx) {
		__sync_synchronize();
		//disk.used->ring[disk.used_idx % NUM].id：获取 "used" 环缓冲区中待处理的完成请求的 ID 编号。
		int id = disk.used->ring[disk.used_idx % NUM].id;//NUM："used" 环中缓冲区条目（entry）的数量。

		//disk.info[id].status：获取块缓冲区完成请求的状态字。
		if (disk.info[id].status != 0)
			panic("virtio_disk_intr status");
//buf：block buffer（块缓冲区）结构体类型，用于组织 VIRTIO 块设备 IO 操作的空间。它是与缓冲区相关的操作的基本数据结构。
		struct buf *b = disk.info[id].b;	//disk.info[id].b：获取块缓冲区指针，即获取需要处理的缓冲区。
		//将当前缓冲区的 disk 标志位清零，表明该缓冲区已经完成 IO 操作，系统可以继续使用该缓冲区。
		b->disk = 0; // disk is done with buf	将b->disk置0
		//disk.used_idx += 1：指向下一个待处理完成请求的下标。
		disk.used_idx += 1;
	}
}
