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

void binit()
{
	struct buf *b;
	// Create linked list of buffers
	bcache.head.prev = &bcache.head;
	bcache.head.next = &bcache.head;
	for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
static struct buf *bget(uint dev, uint blockno)
{
	struct buf *b;
	// Is the block already cached?
	for (b = bcache.head.next; b != &bcache.head; b = b->next) {
		if (b->dev == dev && b->blockno == blockno) {
			b->refcnt++;
			return b;
		}
	}
	// Not cached.
	// Recycle the least recently used (LRU) unused buffer.
	for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
		if (b->refcnt == 0) {
			b->dev = dev;
			b->blockno = blockno;
			b->valid = 0;
			b->refcnt = 1;
			return b;
		}
	}
	panic("bget: no buffers");
	return 0;
}

const int R = 0;
const int W = 1;

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

//brelse 的数量必须和 bget 相同，因为 bget 会使得引用计数加一。
//如果没有相匹配的 brelse，就好比 new 了之后没有 delete。
void brelse(struct buf *b)
{
	b->refcnt--;
	if (b->refcnt == 0) {
		// no one is waiting for it.
		b->next->prev = b->prev;
		b->prev->next = b->next;
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}
}

void bpin(struct buf *b)
{
	b->refcnt++;
}

void bunpin(struct buf *b)
{
	b->refcnt--;
}
