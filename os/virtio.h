//新增，用来处理磁盘中断

#ifndef VIRTIO_H
#define VIRTIO_H

#include "bio.h"

//
// virtio device definitions.
// for both the mmio interface, and virtio descriptors.
// only tested with qemu.
// this is the "legacy" virtio interface.
//这是 virtio 设备的定义，用于描述 mmio 接口和 virtio 描述符，仅在 qemu 中进行了测试。这是“legacy” virtio 接口。
// the virtio spec:		vitrio规范
// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
//

// virtio mmio control registers, mapped starting at 0x10001000.
// from qemu virtio_mmio.h
//virtio mmio 控制寄存器，映射起始地址为 0x10001000。这些定义来自于 qemu 中的 virtio_mmio.h 文件。
#define VIRTIO_MMIO_MAGIC_VALUE 0x000 // 0x74726976	virtio 魔数，固定值0x74726976，用于校验设备是否支持virtio协议。
#define VIRTIO_MMIO_VERSION 0x004 // version; 1 is legacy		virtio 协议版本号，当前仅支持版本 1。
#define VIRTIO_MMIO_DEVICE_ID 0x008 // device type; 1 is net, 2 is disk		设备类型，当前支持的类型有1（网络）和2（磁盘）。
#define VIRTIO_MMIO_VENDOR_ID 0x00c // 0x554d4551		供应商ID，固定值为0x554d4551。
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010		//设备支持的特性，由驱动程序发起读取请求，设备返回支持的特性。
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020		//驱动支持的特性，由驱动程序发起写入请求，告知设备其支持的特性。
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028 // page size for PFN, write-only	//客户机页面大小，仅用于物理页帧编号（PFN）操作，只能写入。
#define VIRTIO_MMIO_QUEUE_SEL 0x030 // select queue, write-only		队列选择器，用于选择当前操作的队列编号，只能写入。
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034 // max size of current queue, read-only	当前队列的最大长度，只读。
#define VIRTIO_MMIO_QUEUE_NUM 0x038 // size of current queue, write-only	当前队列的长度，只能写入。
#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c // used ring alignment, write-only	使用环的对齐方式，只能写入。
#define VIRTIO_MMIO_QUEUE_PFN                                                  \
	0x040 // physical page number for queue, read/write	当前队列对应的PFN，用于确定队列在内存中的位置，可读可写。
#define VIRTIO_MMIO_QUEUE_READY 0x044 // ready bit		队列准备就绪的标志位，只能写入。
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050 // write-only	用于通知设备或驱动程序的信号号码，只能写入。
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060 // read-only	中断状态寄存器，只读。
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064 // write-only		中断确认寄存器，只能写入。
#define VIRTIO_MMIO_STATUS 0x070 // read/write				设备状态寄存器，读写。其中，

// status register bits, from qemu virtio_config.h
////四个常量宏定义了设备状态寄存器的取值，表示设备目前的状态。
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1			//表示设备正在被操作系统识别，还未准备好。
#define VIRTIO_CONFIG_S_DRIVER 2					//表示操作系统正在加载virtio设备的驱动程序。
#define VIRTIO_CONFIG_S_DRIVER_OK 4				//表示virtio设备的驱动程序已经加载完成，设备已经准备好可以使用。
#define VIRTIO_CONFIG_S_FEATURES_OK 8			//表示驱动程序已经完成了其虚拟化特性和设备特性之间的协商过程，可以开始正常通信。

// device feature bits
#define VIRTIO_BLK_F_RO 5 /* Disk is read-only */		//表示磁盘设备是只读的。
#define VIRTIO_BLK_F_SCSI 7 /* Supports scsi command passthru */	//表示磁盘设备支持 SCSI 命令穿透。
#define VIRTIO_BLK_F_CONFIG_WCE 11 /* Writeback mode available in config */	//表示磁盘设备支持写回模式。
#define VIRTIO_BLK_F_MQ 12 /* support more than one vq */	//表示磁盘设备支持多个虚拟队列。
#define VIRTIO_F_ANY_LAYOUT 27		//表示环描述符可以有任意的布局。
#define VIRTIO_RING_F_INDIRECT_DESC 28	//表示间接描述符是有效的，即描述符可以指向PPN而非直接指向物理地址。
#define VIRTIO_RING_F_EVENT_IDX 29	//表示使用事件索引寄存器，通知驱动程序哪些描述符已经就绪。

// this many virtio descriptors.
// must be a power of two.
#define NUM 8	//表示每个virtio设备的队列中描述符的个数，必须是2的幂次方。

// a single descriptor, from the spec.
//表示 virtio 设备中的一个描述符
struct virtq_desc {
	uint64 addr;		//表示数字地址
	uint32 len;			// 表示大小
	uint16 flags;
	uint16 next;		//表示下一个描述符的索引号
};
#define VRING_DESC_F_NEXT 1 // chained with another descriptor	表示该描述符与下一个描述符相关联。
#define VRING_DESC_F_WRITE 2 // device writes (vs read)	表示该描述符关联的数据是写入设备还是从设备读取。

// the (entire) avail ring, from the spec.
//表示 virtio 设备中的 "avail" 环，主机可在其中查找当前是否有请求需处理
struct virtq_avail {
	uint16 flags; // always zero		flags（标识符，设置为0）
	uint16 idx; // driver will write ring[idx] next	下一个可供使用的索引号
	uint16 ring[NUM]; // descriptor numbers of chain heads	带有NUM个元素的ring数组，其中每个元素都指引至 virtq_desc 结构的描述符号。
	uint16 unused;	//表示留空的描述符号
};

// one entry in the "used" ring, with which the
// device tells the driver about completed requests.
//表示 virtio 设备中的 "used" 环上的一项，用于告诉驱动程序有多少个请求已经完成。
struct virtq_used_elem {
	uint32 id; // index of start of completed descriptor chain	//即链式描述符中当前描述符的索引号
	uint32 len;		//当前请求处理的字节数
};

//表示 virtio 设备中的 "used" 环
struct virtq_used {
	uint16 flags; // always zero	标识符，设置为0
	uint16 idx; // device increments when it adds a ring[] entry	下一个未使用的索引号
	struct virtq_used_elem ring[NUM];	//带有 NUM 个元素的 virtq_used_elem 结构体。该数组中的每个元素都表示 virtq_desc 中的一组描述符已被设备完成。
};

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

#define VIRTIO_BLK_T_IN 0 // read the disk		表示磁盘设备读取操作。
#define VIRTIO_BLK_T_OUT 1 // write the disk	表示磁盘设备写入操作。

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
//表示virtio磁盘设备的请求结构
struct virtio_blk_req {
	uint32 type; // VIRTIO_BLK_T_IN or ..._OUT	表示操作类型：VIRTIO_BLK_T_IN或VIRTIO_BLK_T_OUT
	uint32 reserved;	//reserved（预留位）
	uint64 sector;		//sector（表示磁盘扇区地址）此外，两个额外的描述符在 struct virtq_desc 中用于包含块和一个单字节的状态。
};

void virtio_disk_init();	//初始化virtio磁盘设备，包括创建并初始化设备描述符，初始化虚拟队列等。
void virtio_disk_rw(struct buf *, int);	//向virtio磁盘设备提交读/写请求。
void virtio_disk_intr();	//处理virtio磁盘设备的中断请求，包括通知驱动程序已经完成的请求并且需要处理的新请求。

#endif // VIRTIO_H