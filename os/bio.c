// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//缓冲区高速缓存。
//
// 缓冲区高速缓存是一个链接的buf结构体链表，持有磁盘块内容的缓存副本。
// 在内存中缓存磁盘块可以减少磁盘读取的次数，并提供一个同步点，用于多个进程使用的磁盘块。
//
// 接口：
// * 要获取特定磁盘块的缓冲区，请调用bread。
// * 更改缓冲区数据后，请调用bwrite将其写入磁盘。
// * 使用缓冲区后，请调用brelse释放缓冲区。
// * 调用brelse后，请勿再使用缓冲区。
// * 同一时间只有一个进程可以使用一个缓冲区，
//     因此不要保留它们的时间比必要的时间长。

//新增，IO buffer 的实现

/*
为了加快磁盘访问的速度，在内核中设置了磁盘缓存struct buf，一个buf对应一个磁盘block，
这一部分代码也不要求同学们深入掌握。大致的作用机制是，对磁盘的读写都会被转化为对buf的读写，
当buf有效时，读写buf，buf无效时（类似页表缺页和TLB缺失），就实际读写磁盘，将buf变得有效，然后继续读写buf。
buf写回的时机是buf池满需要替换的时候(类似内存的swap策略) 手动写回。
如果buf没有写回，一但掉电就GG了，所以手动写回还是挺重要的。
*/

#include "bio.h"
#include "defs.h"
#include "fs.h"
#include "riscv.h"
#include "types.h"
#include "virtio.h"

struct {
	struct buf buf[NBUF];
	struct buf head;
} bcache;

//用于初始化缓冲区。这个函数做的事情是，将缓冲区数组构建成循环双向链表的形式，
//链表的头节点位于 bcache.head 处，每个节点都是 struct buf 类型的一个缓存块。
void binit()
{
	struct buf *b;
	// Create linked list of buffers
	//创建缓冲区链表、
	//将缓冲区数组构建成循环双向链表的形式
	//头节点的前驱和后继均指向自身，表示链表为空
	bcache.head.prev = &bcache.head;
	bcache.head.next = &bcache.head;
	for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
		//连接前驱和后继节点
		//双向链表中b是当前节点，它的下一个节点为“头节点的下一个节点”，上一个节点为“头节点”
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		//将头节点的后继节点和 b 相连
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
//通过缓冲区高速缓存查找设备 dev 上的块。
//如果没有找到，分配一个缓冲区。
static struct buf *bget(uint dev, uint blockno)
{
	struct buf *b;
	// Is the block already cached?
	// 检查该块是否已经被缓存
	for (b = bcache.head.next; b != &bcache.head; b = b->next) {
		if (b->dev == dev && b->blockno == blockno) {
			b->refcnt++;	// 增加引用计数以表示该缓冲区正在被使用。
			return b;
		}
	}
	// Not cached.
	// Recycle the least recently used (LRU) unused buffer.
	//没有缓存。
	//回收最近最少使用 (LRU) 未使用的缓冲区。
	
	// 寻找最近最少使用 (LRU) 的未使用缓冲区
	for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {	//从后往前遍历寻找空的buf
		if (b->refcnt == 0) {
			// 重用该缓冲区来缓存新块号
			b->dev = dev;
			b->blockno = blockno;
			b->valid = 0;		// 由于尚未从磁盘中读取数据，因此将其标记为无效。
			b->refcnt = 1;	// 更新引用计数，以表示该缓冲区正在被使用。
			return b;
		}
	}
	// 如果没有找到未使用的缓冲区，则触发 panic 操作。
	panic("bget: no buffers");
	return 0;
}

const int R = 0;	//读操作
const int W = 1;	//写操作

// Return a buf with the contents of the indicated block.
//返回一个包含指定块内容的 buf。
//读取文件数据实际就是读取文件inode指向数据块的数据。读数据块到缓存的数据需要使用bread
struct buf *bread(uint dev, uint blockno)
{
	struct buf *b;
	//使用bget去查缓存中是否已有对应的block，如果没有会分配内存来缓存对应的块。
	b = bget(dev, blockno);
	if (!b->valid) {
		virtio_disk_rw(b, R);
		b->valid = 1;
	}
	return b;
}

// Write b's contents to disk.
//将 b 的内容写入磁盘。
//写回缓存需要用到bwrite函数
void bwrite(struct buf *b)
{
	virtio_disk_rw(b, W);
}

// Release a buffer.
// Move to the head of the most-recently-used list.
//释放缓冲区。
//移动到最近使用列表的头部。
//释放块缓存的brelse函数。

//brelse 不会真的如字面意思释放一个 buf。它的准确含义是暂时不操作该 buf 了并把它放置在bcache链表的首部，
//buf的真正释放会被推迟到buf池满，无法分配的时候，就会把最近最久未使用的buf释放掉（释放 = 写回 + 清空）。

//brelse的数量必须和bget相同，因为bget会使得引用计数加一。
//如果没有相匹配的brelse，就好比new了之后没有delete。
void brelse(struct buf *b)
{
	// 对缓冲区的引用计数进行减一。
	b->refcnt--;
	if (b->refcnt == 0) {	// 如果没有其他地方继续引用该缓冲区。
		// no one is waiting for it.
		
		// 从 LRU 缓存中移除该缓冲区。
		b->next->prev = b->prev;
		b->prev->next = b->next;
		// 将其插入缓存列表的前方。
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}
}

/*
引用计数用于控制缓冲区的使用情况。当引用计数为 0 时，表示该缓冲区没有被任何代码使用，可以被回收和重新利用，
用于缓存其他块的数据。当引用计数为正数时，表示该缓冲区正在被占用，不能被回收。
bpin() 和 bunpin() 函数根据缓冲区的使用情况增加或减少引用计数。
*/
//bpin() 函数用于增加缓冲区的引用计数，在使用缓冲区时调用。
void bpin(struct buf *b)
{
	// 增加缓冲区的引用计数。
	b->refcnt++;
}

//bunpin() 函数则将缓冲区的引用计数减少一，当不再需要该缓冲区时调用。
void bunpin(struct buf *b)
{
	// 减少缓冲区的引用计数。
	b->refcnt--;
}
