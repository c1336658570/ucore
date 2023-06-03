// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.
//文件系统实现。五层：
//+ Blocks：原始磁盘块的分配器。
//+ 日志：用于多步更新的崩溃恢复。
//+ 文件：inode 分配器、读、写、元数据。
//+ 目录：具有特殊内容的索引节点（其他索引节点列表！）
//+ 名称：类似/usr/rtm/xv6/fs.c 的路径，方便命名。
//这个文件包含了低级别的文件系统操作函数。 （更高级别的）系统调用实现在sysfile.c中。

//新增，文件系统实际逻辑
#include "fs.h"
#include "bio.h"
#include "defs.h"
#include "file.h"
#include "proc.h"
#include "riscv.h"
#include "types.h"
// there should be one superblock per disk device, but we run with
// only one device
//每个磁盘设备应该有一个超级块，但我们只使用一个设备。
struct superblock sb;

// Read the super block.
//读取超级块。
/*
readsb() 是一个读取文件系统超级块（SuperBlock）的函数。具体地，该函数首先通过调用bread()
函数读取指定设备(dev)上的第1块数据，并通过 bp 将块中数据读入内存（bp 结构体后跟着对应磁盘块的数据，
即 bp.data 指向该块数据的起始地址）。然后，使用 memmove()函数将该缓存块bp中的超级块数据拷贝到指向超级块的结构体指针sb 所指向的内存空间中，函数参数中的 *sb 即为超级块结构体的指针。最后，使用 brelse() 函数释放缓存块的内存及其他相关结构体。
*/
static void readsb(int dev, struct superblock *sb)
{
	struct buf *bp;           // 定义指向缓存块的指针变量 bp
	bp = bread(dev, 1);   // 调用函数 bread() 读取指定设备(dev)上的第1个块，保存到 bp 缓存块中
	memmove(sb, bp->data, sizeof(*sb));  // 使用 memmove() 函数将缓存中读取到的超级块数据 bp->data 拷贝到 sb 指向的空间中
	brelse(bp);               // 调用函数 brelse() 释放缓存块内存及所有相关结构体
}

// Init fs
//初始化文件系统
/*
于初始化文件系统，并通过readsb()函数读取文件系统超级块（SuperBlock）信息。如果文件系统的魔数不匹配
（即文件系统不合法），则会调用panic()函数终止程序运行。如果文件系统合法，则可以进行后续的文件操作。
*/
void fsinit()
{
	int dev = ROOTDEV;    // 将 ROOTDEV 宏定义的设备编号赋值给 dev 变量，指定要操作的文件系统所在设备
	readsb(dev, &sb);     // 调用函数 readsb() 读取设备(dev)上的超级块数据，并将它们存储到全局变量 sb 中（指向整个文件系统的超级块）
	if (sb.magic != FSMAGIC) {    // 根据全局变量 sb 中的魔数（magic）判断文件系统是否合法
		panic("invalid file system");   // 如果文件系统不合法，则调用函数 panic() 终止程序运行
	}
}

// Zero a block.
//零块。
/*
这段代码实现了将一个物理块清零的操作。具体地，该函数首先通过调用bread函数读取设备(dev)上的块号(bno)对应的缓存块(bp)。
然后，通过使用memset函数设置缓存块(bp)的全部数据为0, 完成清空操作。
接着，调用bwrite()函数将缓存块(bp)的内容写回设备(dev)，完成物理块更新。
最后，使用brelse()函数释放缓存块的内存及其他相关结构体。
*/
static void bzero(int dev, int bno)
{
	struct buf *bp;           // 定义指向缓存块的指针变量 bp
	bp = bread(dev, bno);     // 调用函数 bread 读取逻辑块 bno 对应的物理块到缓存中，并将缓存块的指针赋值给 bp
	memset(bp->data, 0, BSIZE);  // 使用函数 memset 将缓存块所对应的内存区域清零，即将缓存块中的数据全部置为 0
	bwrite(bp);               // 调用函数 bwrite 将缓存块写回磁盘
	brelse(bp);               // 调用函数 brelse 释放缓存块内存及所有相关结构体
}

// Blocks.

// Allocate a zeroed disk block.
// 分配一个被清零的磁盘块
/*
该函数用于在文件系统中找到一个未被使用过的磁盘块，并返回该块的块号。其过程通过遍历磁盘上的所有块找到第一个可用的块，
并将其标记为已使用。函数的参数 dev 为指定的设备号（Device Number），用于确定磁盘块的位置；
sb 是指向超级块（SuperBlock）的指针，用于确定磁盘块的总大小。
函数首先初始化一个缓存块 bp，然后使用bread函数读取其中的数据。之后，它以BPB为步长遍历所有磁盘块，
对每个磁盘块进行处理。在遍历磁盘块时，它使用位运算检查每个位是否已被分配，并在找到一个空闲块后将其标记为已使用，
在写入磁盘后返回该块。若已无空闲块可用，则使用panic函数终止程序执行。
*/
static uint balloc(uint dev)
{
	int b, bi, m;              // 用于保存磁盘块的块号、位编号和位掩码
	struct buf *bp;            // 定义指向缓存块的指针变量 bp，用于读取磁盘块

	bp = 0;
	// 遍历整个文件系统上的块，以 BPB 为步长循环
	for (b = 0; b < sb.size; b += BPB) {
		bp = bread(dev, BBLOCK(b, sb));     // 调用函数 bread 读出磁盘块对应的数据，存储在缓存块 bp 中
		// 遍历缓存块的每一个位，从低到高判断是否有空闲磁盘块可用
		for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
			m = 1 << (bi % 8);             // 计算当前位所在的字节对应的位掩码
			// 判断当前位是否空闲，如果是则分配该块，并将其标记为已使用
			if ((bp->data[bi / 8] & m) == 0) {   
				bp->data[bi / 8] |= m;
				bwrite(bp);                 // 将缓存块的更改写回磁盘，确保已分配的块存储空间不会再被释放出去
				brelse(bp);                 // 释放该缓存块
				bzero(dev, b + bi);         // 清空该块内容，以确保该块中的数据不会出现脏数据
				return b + bi;              // 返回已分配的磁盘块编号
			}
		}
		brelse(bp);             // 如果该磁盘块不可用，则释放该缓存块 bp
	}
	panic("balloc: out of blocks"); // 如果磁盘块已分配完毕，则调用 panic 终止执行
	return 0;   // 如果程序正确执行，则始终不会执行到这里
}

// Free a disk block.
// 释放一个磁盘块
/*
该函数用于释放磁盘上的指定块，其中dev和b参数分别指定设备（Device Number）和要释放的块号。
函数首先通过bread函数将要释放的块读入缓冲区。接着，它检查该块是否已经被释放。
如果没有，则修改它并标记为空闲，这个过程包括计算磁盘块中位图中哪个位对应该块，
并将该位图的“使用”位标记为“未使用状态”。最后，使用bwrite()函数将缓存块的更新写回磁盘，并使用brelse()释放该块的缓存。
总之，该函数用于确保文件系统上的空闲块在需要时可以使用。该函数通常是在文件删除或压缩文件中分配空间时自动调用的。
*/
static void bfree(int dev, uint b)
{
	struct buf *bp;    // 定义指向缓存块的指针变量 bp，用于读取和修改磁盘块
	int bi, m;         // 用于保存磁盘块中某个位的编号和位掩码

	bp = bread(dev, BBLOCK(b, sb));   // 通过调用 bread 函数读取磁盘块的缓存块
	bi = b % BPB;                     // 计算该磁盘块在当前缓存块中的偏移量（位编号）
	m = 1 << (bi % 8);                // 计算当前位所在的字节对应的位掩码
	if ((bp->data[bi / 8] & m) == 0) {  // 如果该块已经是空闲的，则报错，并调用 panic 函数终止程序
			panic("freeing free block");
	}
	bp->data[bi / 8] &= ~m;           // 将当前位标记为空闲状态，即将掩码 m 转换为反码（同 `& ~m`），然后再进行按位与操作
	bwrite(bp);                       // 将缓存块中的数据写回磁盘
	brelse(bp);                       // 释放缓存块的内存及其他相关结构体
}

//The inode table in memory
//内存中的inode表
struct {
	struct inode inode[NINODE];
} itable;

static struct inode *iget(uint dev, uint inum);

// Allocate an inode on device dev.
// Mark it as allocated by  giving it type `type`.
// Returns an allocated and referenced inode.
// 在设备 dev 上分配一个 inode，将其标记为已分配并赋予类型 type，并返回已分配和引用的 inode。
//ialloc干的事情：遍历inode blocks找到一个空闲的inode，初始化并返回。
/*
该函数用于在磁盘上分配一个 inode 并将其标记为 “已分配”，因此它被称为 inode 分配器（ialloc()）。
函数的参数 dev 是设备号（Device Number），type 是指定 inode 的类型。函数首先遍历整个inode位图以查找空闲的inode。
然后，它使用memset()函数将该 inode 的数据清零，将其类型标记为已分配，然后使用bwrite()函数将该块的更新写入磁盘。
最后，它通过调用iget()函数获取该 inode 的指针，并返回它。
在文件系统中，每个文件和目录都有一个 inode。每当用户在文件系统中创建一个新文件时，
就需要在 inode 位图上分配一个 inode。因此，该函数用于确保文件系统上的空闲 inode
*/
struct inode *ialloc(uint dev, short type)
{
	int inum;                   // 定义用于保存 inode 编号的变量 inum
	struct buf *bp;             // 定义指向缓存块的指针变量 bp，用于读取 inode 数据
	struct dinode *dip;         // 定义指向磁盘 inode 块中特定 inode 数据的结构体指针 dip

	// 遍历整个 inode 的数据块，以 IPB 为步长循环
	for (inum = 1; inum < sb.ninodes; inum++) {
		bp = bread(dev, IBLOCK(inum, sb));  // 通过调用 bread() 函数读取 inode 所属的数据块
		dip = (struct dinode *)bp->data + inum % IPB;   // 根据 IPB 计算出特定 inode 数据的偏移量
		if (dip->type == 0) {   // 如果 inode 标记为空闲
			memset(dip, 0, sizeof(*dip));    // 将 inode 块中该 inode 的数据清零
			dip->type = type;                 // 将 inode 块中该 inode 的类型设置为已分配
			bwrite(bp);                       // 将 inode 块的更改写入磁盘
			brelse(bp);                       // 释放缓存块的内存及其他相关结构体
			return iget(dev, inum);           // 根据 inode 编号调用 iget 函数获取该 inode，并返回它的指针
		}
		brelse(bp);  // 将缓存块的内存释放掉，准备处理下一个块
	}
	panic("ialloc: no inodes");  // 如果 inode 已分配完毕，则调用 panic 终止执行
	return 0;   // 如果程序正确执行，则始终不会执行到这里
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// 将修改过的内存中 inode 的信息写回到磁盘中。
// 在修改任何存储在磁盘上的 ip->xxx 字段之后，必须调用此函数。
//balloc(位于nfs/fs.c)会分配一个新的buf缓存。
//而iupdate函数则是把修改之后的inode重新写回到磁盘上。不然掉电了就凉了。
void iupdate(struct inode *ip)
{
	struct buf *bp;		// 缓存块指针，用于读取和写入磁盘块
	struct dinode *dip;	// 磁盘上的inode指针

	bp = bread(ip->dev, IBLOCK(ip->inum, sb)); // 读取inode所在的磁盘块
	dip = (struct dinode *)bp->data + ip->inum % IPB; // 计算inode在磁盘块中的偏移，并将dip指向该位置
	dip->type = ip->type; // 将内存中的inode类型复制到磁盘上
	dip->size = ip->size; // 将内存中的inode大小复制到磁盘上
	// LAB4: 可能需要在这里更新链接计数
	memmove(dip->addrs, ip->addrs, sizeof(ip->addrs)); // 将内存中的inode数据块地址复制到磁盘上
	bwrite(bp); // 将缓存块写回磁盘
	brelse(bp); // 释放缓存块
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not read
// it from disk.
// 根据设备dev和inode编号inum在表中查找inode的内存副本，并返回内存中的副本，不会从磁盘读取它。
// 找到inum号dinode绑定的inode，如果不存在新绑定一个
static struct inode *iget(uint dev, uint inum)
{
	// 声明变量 empty，用于记录表中空闲槽的位置
	// 声明变量 ip 用于在 inode table 中遍历查找
	struct inode *ip, *empty;
	// Is the inode already in the table?
	//inode 已经在表中了吗？
	// 遍历查找 inode table
	empty = 0;
	for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {	 // 遍历查找 inode table
		// 如果有对应的，引用计数 +1并返回inode内存副本
		if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
			ip->ref++;
			return ip;
		}
		if (empty == 0 && ip->ref == 0) // Remember empty slot.	//记住空槽。
			empty = ip;
	}

	// 如果 inode table 已满，即没有 empty 空闲槽，发出panic警告
	// Recycle an inode entry.
	//回收 inode 条目。
	//GG，inode表满了，果断自杀.lab7正常不会出现这个情况。
	if (empty == 0)
		panic("iget: no inodes");

	//创建新的 inode 内存副本
	//注意这里仅仅是写了元数据，没有实际读取，实际读取推迟到后面	
	ip = empty;
	ip->dev = dev;
	ip->inum = inum;
	ip->ref = 1;
	ip->valid = 0;		//没有实际读取，valid = 0	表明还没有读入inode的内容俄
	return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
// 增加inode的引用计数。
// 返回ip以启用ip = idup(ip1)的形式。
struct inode *idup(struct inode *ip)	//struct inode *ip：指向内存中的inode的指针，表示要增加引用计数的inode
{
	ip->ref++; // 增加inode的引用计数
	return ip; // 返回inode指针
}

// Reads the inode from disk if necessary.
//如有必要，从磁盘读取索引节点。
//当已经得到一个文件对应的inode 后，可以通过ivalid函数确保其是有效的。
/*
读取磁盘上的inode内容，并将其复制到对应的内存inode中。如果该inode已经被读取，则不执行任何操作。
*/
void ivalid(struct inode *ip)	//指向内存中inode的指针
{
	struct buf *bp;	 // 缓存块指针，用于读取和写入磁盘块
	struct dinode *dip;	// 磁盘上的inode指针
	if (ip->valid == 0) {	 // 如果该inode还没有被读取
		// bread可以完成一个块的读取，这个在讲buf的时候说过了
		// IBLOCK可以计算inum在几个block
		bp = bread(ip->dev, IBLOCK(ip->inum, sb));	// 读取inode所在的磁盘块
		// 得到dinode内容
		dip = (struct dinode *)bp->data + ip->inum % IPB;	// 计算inode在磁盘块中的偏移，并将dip指向该位置
		// 完成实际读取
		ip->type = dip->type;		 // 将磁盘上的inode类型复制到对应的内存inode
		ip->size = dip->size;			// 将磁盘上的inode大小复制到对应的内存inode
		// LAB4: You may need to get lint count here
		// LAB4：在这里可能需要获取链接计数
		memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));	// 将磁盘上的inode数据块地址复制到对应的内存inode
		// buf暂时没用了
		brelse(bp);	// 释放缓存块
		// 现在有效了
		ip->valid = 1;	 // 标记该inode已经被读取
		if (ip->type == 0)	// 如果inode没有类型，则触发panic警告
			panic("ivalid: no type");
	}
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
// 释放对inode的引用。如果该inode是最后一次引用，则可以回收inode table条目。
//如果该inode是最后一次引用且没有链接到它，就释放磁盘上的索引节点（及其内容）。
//所有对 iput() 的调用都必须在一个事务内
//如果它必须释放 inode。
void iput(struct inode *ip)
{
	// LAB4: Unmark the condition and change link count variable name (nlink) if needed
	
	// 在这里检查是否是最后一次引用，并且该inode没有链接到其他文件，此处注释掉了判断链接计数的代码
	if (ip->ref == 1 && ip->valid && 0 /*&& ip->nlink == 0*/) {
		// inode has no links and no other references: truncate and free.
		// inode没有链接到其他文件且没有其他引用，则截断和释放
		itrunc(ip);		 // 截断和释放inode占用的数据块
		ip->type = 0;		 // 设置inode类型为0
		iupdate(ip);		// 更新inode内容到磁盘上
		ip->valid = 0;	 // 标记该inode没有被读取
	}
	ip->ref--;			// 减少对inode的引用
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].
// i节点内容
//
// 与每个i节点关联的内容（数据）以块的形式存储在磁盘上。
// 前NDIRECT个块编号在ip->addrs[]中列出。
// 下一个NINDIRECT块在块ip->addrs[NDIRECT]中列出。

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
// 返回i节点ip中第n个块的磁盘块地址。如果没有这样的块，则bmap分配一个。
static uint bmap(struct inode *ip, uint bn)
{
	uint addr, *a;
	struct buf *bp;
	// 如果 bn < 12，属于直接索引, block num = ip->addr[bn]
	if (bn < NDIRECT) {
		if ((addr = ip->addrs[bn]) == 0)	//如果对应的磁盘块地址为0，需要再分配一个空闲块
			// 如果对应的 addr, 也就是　block num = 0，表明文件大小增加，需要给文件分配新的 data block
			// 这是通过 balloc 实现的，具体做法是在 bitmap 中找一个空闲 block，置位后返回其编号
			ip->addrs[bn] = addr = balloc(ip->dev);	//balloc(位于nfs/fs.c)会分配一个新的buf缓存。而iupdate函数则是把修改之后的inode重新写回到磁盘上。不然掉电了就凉了。
		return addr;
	}
	// 接下来处理间接索引块
	bn -= NDIRECT;	// 转换为相对于间接索引块的索引
// 间接索引块，那么对应的数据块就是一个大　addr 数组。
	if (bn < NINDIRECT) {	// 如果n在间接索引块可以覆盖的范围内
		// Load indirect block, allocating if necessary.
		//加载间接块，必要时分配。
		if ((addr = ip->addrs[NDIRECT]) == 0)	// 如果间接索引块未分配，则分配一个
			ip->addrs[NDIRECT] = addr = balloc(ip->dev);	// 通过balloc函数在设备上分配一个空闲块
		bp = bread(ip->dev, addr);	// 读取间接索引块到缓存中
		a = (uint *)bp->data;		// 将缓存数据转换为uint类型指针
		if ((addr = a[bn]) == 0) {	// 如果指定的数据块还没有被分配
			a[bn] = addr = balloc(ip->dev);	// 在设备上分配一个新的空闲块，修改间接索引块中的指针
			bwrite(bp);	// 将间接索引块写回磁盘
		}
		brelse(bp);	// 释放缓存
		return addr;	// 返回指定的数据块地址
	}

	panic("bmap: out of range");
	return 0;
}

// Truncate inode (discard contents).
// 截断inode（丢弃内容）。
void itrunc(struct inode *ip)
{
	int i, j;
	struct buf *bp;
	uint *a;

	for (i = 0; i < NDIRECT; i++) {	// 释放直接索引块
		if (ip->addrs[i]) {
			bfree(ip->dev, ip->addrs[i]);	// 通过bfree函数释放直接索引块
			ip->addrs[i] = 0;	// 将对应的磁盘块地址置为0
		}
	}

	if (ip->addrs[NDIRECT]) {	// 如果存在间接索引块
		bp = bread(ip->dev, ip->addrs[NDIRECT]);	// 读取间接索引块到缓存中
		a = (uint *)bp->data;	// 将缓存数据转换为uint类型指针
		for (j = 0; j < NINDIRECT; j++) {	// 释放间接索引块引用的磁盘块
			if (a[j])
				bfree(ip->dev, a[j]);	// 通过bfree函数释放磁盘块
		}
		brelse(bp);		// 释放缓存
		bfree(ip->dev, ip->addrs[NDIRECT]);		// 释放间接索引块
		ip->addrs[NDIRECT] = 0;		// 将间接索引块的磁盘块地址置为0
	}

	ip->size = 0;
	iupdate(ip);		// 更新inode内容到磁盘上
}

// Read data from inode.
// If user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
//从inode读取数据。
//如果user_dst==1，则dst为用户虚拟地址；
//否则，dst是内核地址。
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
	uint tot, m;
	// 还记得 buf 吗？
	struct buf *bp;

	if (off > ip->size || off + n < off)	//off大于文件总字节或n < 0都直接返回
		return 0;
	if (off + n > ip->size)	//off+n比总字节大，直接返回
		n = ip->size - off;

	for (tot = 0; tot < n; tot += m, off += m, dst += m) {
		// bmap 完成 off 到 block num 的对应，见下
		bp = bread(ip->dev, bmap(ip, off / BSIZE));
		// 一次最多读一个块，实际读取长度为 m
		m = MIN(n - tot, BSIZE - off % BSIZE);
		if (either_copyout(user_dst, dst,
				   (char *)bp->data + (off % BSIZE), m) == -1) {
			brelse(bp);
			tot = -1;
			break;
		}
		brelse(bp);
	}
	return tot;
}

// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
//向 inode 写入数据。
//调用者必须持有 ip->lock。
//如果user_src==1，则src为用户虚拟地址；
//否则，src 是内核地址。
//返回成功写入的字节数。
//如果返回值小于请求的n，
//说明出现了某种错误。
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
	uint tot, m;
	struct buf *bp;

	if (off > ip->size || off + n < off)	//off大于文件总字节或n < 0都直接返回
		return -1;
	if (off + n > MAXFILE * BSIZE)
		return -1;

	for (tot = 0; tot < n; tot += m, off += m, src += m) {
		bp = bread(ip->dev, bmap(ip, off / BSIZE));
		m = MIN(n - tot, BSIZE - off % BSIZE);
		if (either_copyin(user_src, src,
				  (char *)bp->data + (off % BSIZE), m) == -1) {
			brelse(bp);
			break;
		}
		bwrite(bp);
		brelse(bp);
	}

	// 文件长度变长，需要更新 inode 里的 size 字段
	if (off > ip->size)
		ip->size = off;

	// write the i-node back to disk even if the size didn't change
	// because the loop above might have called bmap() and added a new
	// block to ip->addrs[].
	// 即使大小没有更改，也将i节点写回磁盘，
	// 因为上面的循环可能会调用bmap()并向ip->addrs[]添加新块。
	// 有可能inode信息被更新了，写回
	iupdate(ip);

	return tot;
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
//在目录中查找一个目录项。
//如果找到，则将 *poff 设置为该目录项的字节偏移量。
//遍历这个inode索引的数据块中存储的文件信息到dirent结构体之中，比较名称和给定的文件名是否一致。
//遍历根目录所有的dirent，找到name一样的inode。
struct inode *dirlookup(struct inode *dp, char *name, uint *poff)
{
	uint off, inum;
	struct dirent de;

	if (dp->type != T_DIR)
		panic("dirlookup not DIR");

	//每次迭代处理一个block，注意根目录可能有多个data block
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlookup read");
		if (de.inum == 0)
			continue;
		if (strncmp(name, de.name, DIRSIZ) == 0) {
			// entry matches path element
			if (poff)
				*poff = off;
			inum = de.inum;
			//找到之后，绑定一个内存inode然后返回
			return iget(dp->dev, inum);
		}
	}

	return 0;
}

//Show the filenames of all files in the directory
int dirls(struct inode *dp)	//指向包含目录内容的inode的指针
{
	uint64 off, count;	 // off：目录中当前条目的偏移量；count：已经处理的目录条目数
	struct dirent de;		// 目录项

	if (dp->type != T_DIR)	 // 如果传入的inode不是目录，则触发panic警告
		panic("dirlookup not DIR");

	count = 0;
	for (off = 0; off < dp->size; off += sizeof(de)) {	// 遍历目录中所有的条目
		// 读取目录项，如果读取大小不匹配，则触发panic警告
		if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlookup read");
		// 如果该目录项不对应任何inode，则跳过
		if (de.inum == 0)
			continue;
		// 输出目录项中的文件名
		printf("%s\n", de.name);
		count++;	// 增加已经处理的目录项计数
	}
	return count;	 // 返回处理的目录项计数
}

// Write a new directory entry (name, inum) into the directory dp.
//向目录 dp 中写入一个新的目录条目 (name, inum)
/*
dirlink和dirlookup不同，我们没有现成的dirent存储在磁盘上，而是要在磁盘上创建一个新的dirent。
他遍历根目录数据块，找到一个空的 dirent，设置 dirent = {inum, filename} 然后返回，
注意这一步可能找不到空位，这时需要找一个新的数据块，并扩大 root_dir size，这是由bmap自动完成的。
*/
int dirlink(struct inode *dp, char *name, uint inum)
{
	int off;
	struct dirent de;
	struct inode *ip;
	// Check that name is not present.
	//检查名称不存在。
	if ((ip = dirlookup(dp, name, 0)) != 0) {
		iput(ip);
		return -1;
	}

	// Look for an empty dirent.
	//寻找一个空目录。
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlink read");
		if (de.inum == 0)
			break;
	}
	strncpy(de.name, name, DIRSIZ);
	de.inum = inum;
	if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
		panic("dirlink");
	return 0;
}

// LAB4: You may want to add dirunlink here
//LAB4: 你可能想在这里添加 dirunlink

//Return the inode of the root directory
//返回根目录的inode
struct inode *root_dir()
{
	struct inode *r = iget(ROOTDEV, ROOTINO);
	ivalid(r);
	return r;
}

//Find the corresponding inode according to the path
//根据路径找到对应的inode
struct inode *namei(char *path)
{
	int skip = 0;
	// if(path[0] == '.' && path[1] == '/')
	//     skip = 2;
	// if (path[0] == '/') {
	//     skip = 1;
	// }
	//由于我们是单目录结构。因此首先我们调用root_dir获取根目录对应的inode
	struct inode *dp = root_dir();	
	if (dp == 0)
		panic("fs dumped.\n");
	//之后就遍历这个inode索引的数据块中存储的文件信息到dirent结构体之中，比较名称和给定的文件名是否一致。
	return dirlookup(dp, path + skip, 0);
}
