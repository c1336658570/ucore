#ifndef BUF_H
#define BUF_H

#include "fs.h"
#include "types.h"

// 数据块缓存结构体。
struct buf {
	int valid; // has data been read from disk?	标识缓存是否包含从磁盘读取的有效数据，如果有效，则为1，否则为0。
	int disk; // does disk "own" buf?		表示缓存是否存储在磁盘上，如果是，则为 1，否则为 0。
	uint dev;		//指定缓存块对应的磁盘设备号。
	uint blockno;	//指定缓存块在磁盘上的块号。
	uint refcnt;	//引用计数器，记录当前缓存块被多少个进程或线程同时引用。
	//用于形成缓存块的 LRU（最近最少使用）链表，记录缓存块在链表中的前驱和后继。
	struct buf *prev; // LRU cache list
	struct buf *next;
	uchar data[BSIZE];		//缓存块的数据部分，占用BSIZE（块大小）字节的空间。缓存块的实际内容存储在data数组中。
};

void binit(void);
struct buf *bread(uint, uint);
void brelse(struct buf *);
void bwrite(struct buf *);
void bpin(struct buf *);
void bunpin(struct buf *);

#endif // BUF_H
