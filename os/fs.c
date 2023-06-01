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
struct superblock sb;

// Read the super block.
static void readsb(int dev, struct superblock *sb)
{
	struct buf *bp;
	bp = bread(dev, 1);
	memmove(sb, bp->data, sizeof(*sb));
	brelse(bp);
}

// Init fs
void fsinit()
{
	int dev = ROOTDEV;
	readsb(dev, &sb);
	if (sb.magic != FSMAGIC) {
		panic("invalid file system");
	}
}

// Zero a block.
static void bzero(int dev, int bno)
{
	struct buf *bp;
	bp = bread(dev, bno);
	memset(bp->data, 0, BSIZE);
	bwrite(bp);
	brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint balloc(uint dev)
{
	int b, bi, m;
	struct buf *bp;

	bp = 0;
	for (b = 0; b < sb.size; b += BPB) {
		bp = bread(dev, BBLOCK(b, sb));
		for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
			m = 1 << (bi % 8);
			if ((bp->data[bi / 8] & m) == 0) { // Is block free?
				bp->data[bi / 8] |= m; // Mark block in use.
				bwrite(bp);
				brelse(bp);
				bzero(dev, b + bi);
				return b + bi;
			}
		}
		brelse(bp);
	}
	panic("balloc: out of blocks");
	return 0;
}

// Free a disk block.
static void bfree(int dev, uint b)
{
	struct buf *bp;
	int bi, m;

	bp = bread(dev, BBLOCK(b, sb));
	bi = b % BPB;
	m = 1 << (bi % 8);
	if ((bp->data[bi / 8] & m) == 0)
		panic("freeing free block");
	bp->data[bi / 8] &= ~m;
	bwrite(bp);
	brelse(bp);
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
//在设备 dev 上分配一个 i 节点。
//将其标记为已分配并给予类型“type”。
//返回已分配并被引用的 i 节点。
//ialloc干的事情：遍历inode blocks找到一个空闲的inode，初始化并返回。
struct inode *ialloc(uint dev, short type)
{
	int inum;
	struct buf *bp;
	struct dinode *dip;

	for (inum = 1; inum < sb.ninodes; inum++) {
		bp = bread(dev, IBLOCK(inum, sb));
		dip = (struct dinode *)bp->data + inum % IPB;
		if (dip->type == 0) { // a free inode
			memset(dip, 0, sizeof(*dip));
			dip->type = type;
			bwrite(bp);
			brelse(bp);
			return iget(dev, inum);
		}
		brelse(bp);
	}
	panic("ialloc: no inodes");
	return 0;
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// 将已修改的内存inode复制到磁盘。
// 必须在更改ip->xxx字段之后调用，该字段存在于磁盘上。
//balloc(位于nfs/fs.c)会分配一个新的buf缓存。
//而iupdate函数则是把修改之后的inode重新写回到磁盘上。不然掉电了就凉了。
void iupdate(struct inode *ip)
{
	struct buf *bp;
	struct dinode *dip;

	bp = bread(ip->dev, IBLOCK(ip->inum, sb));
	dip = (struct dinode *)bp->data + ip->inum % IPB;
	dip->type = ip->type;
	dip->size = ip->size;
	// LAB4: you may need to update link count here
	memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
	bwrite(bp);
	brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not read
// it from disk.
//在设备dev上找到编号为inum的inode，并返回内存中的副本，不会从磁盘读取它。

// 找到inum号dinode绑定的inode，如果不存在新绑定一个
static struct inode *iget(uint dev, uint inum)
{
	struct inode *ip, *empty;
	// Is the inode already in the table?
	//inode 已经在表中了吗？
	// 遍历查找 inode table
	empty = 0;
	for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
		// 如果有对应的，引用计数 +1并返回
		if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
			ip->ref++;
			return ip;
		}
		if (empty == 0 && ip->ref == 0) // Remember empty slot.	//记住空槽。
			empty = ip;
	}

	// Recycle an inode entry.
	//回收 inode 条目。
	//GG，inode表满了，果断自杀.lab7正常不会出现这个情况。
	if (empty == 0)
		panic("iget: no inodes");
	//注意这里仅仅是写了元数据，没有实际读取，实际读取推迟到后面	
	ip = empty;
	ip->dev = dev;
	ip->inum = inum;
	ip->ref = 1;
	ip->valid = 0;		//没有实际读取，valid = 0
	return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *idup(struct inode *ip)
{
	ip->ref++;
	return ip;
}

// Reads the inode from disk if necessary.
//如有必要，从磁盘读取索引节点。
//当已经得到一个文件对应的inode 后，可以通过ivalid函数确保其是有效的。
void ivalid(struct inode *ip)
{
	struct buf *bp;
	struct dinode *dip;
	if (ip->valid == 0) {
		// bread可以完成一个块的读取，这个在讲buf的时候说过了
		// IBLOCK可以计算inum在几个block
		bp = bread(ip->dev, IBLOCK(ip->inum, sb));
		// 得到dinode内容
		dip = (struct dinode *)bp->data + ip->inum % IPB;
		// 完成实际读取
		ip->type = dip->type;
		ip->size = dip->size;
		// LAB4: You may need to get lint count here
		memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
		// buf暂时没用了
		brelse(bp);
		// 现在有效了
		ip->valid = 1;
		if (ip->type == 0)
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
void iput(struct inode *ip)
{
	// LAB4: Unmark the condition and change link count variable name (nlink) if needed
	if (ip->ref == 1 && ip->valid && 0 /*&& ip->nlink == 0*/) {
		// inode has no links and no other references: truncate and free.
		itrunc(ip);
		ip->type = 0;
		iupdate(ip);
		ip->valid = 0;
	}
	ip->ref--;
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
		if ((addr = ip->addrs[bn]) == 0)
			// 如果对应的 addr, 也就是　block num = 0，表明文件大小增加，需要给文件分配新的 data block
			// 这是通过 balloc 实现的，具体做法是在 bitmap 中找一个空闲 block，置位后返回其编号
			ip->addrs[bn] = addr = balloc(ip->dev);	//balloc(位于nfs/fs.c)会分配一个新的buf缓存。而iupdate函数则是把修改之后的inode重新写回到磁盘上。不然掉电了就凉了。
		return addr;
	}
	bn -= NDIRECT;
// 间接索引块，那么对应的数据块就是一个大　addr 数组。
	if (bn < NINDIRECT) {
		// Load indirect block, allocating if necessary.
		//加载间接块，必要时分配。
		if ((addr = ip->addrs[NDIRECT]) == 0)
			ip->addrs[NDIRECT] = addr = balloc(ip->dev);
		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		if ((addr = a[bn]) == 0) {
			a[bn] = addr = balloc(ip->dev);
			bwrite(bp);
		}
		brelse(bp);
		return addr;
	}

	panic("bmap: out of range");
	return 0;
}

// Truncate inode (discard contents).
void itrunc(struct inode *ip)
{
	int i, j;
	struct buf *bp;
	uint *a;

	for (i = 0; i < NDIRECT; i++) {
		if (ip->addrs[i]) {
			bfree(ip->dev, ip->addrs[i]);
			ip->addrs[i] = 0;
		}
	}

	if (ip->addrs[NDIRECT]) {
		bp = bread(ip->dev, ip->addrs[NDIRECT]);
		a = (uint *)bp->data;
		for (j = 0; j < NINDIRECT; j++) {
			if (a[j])
				bfree(ip->dev, a[j]);
		}
		brelse(bp);
		bfree(ip->dev, ip->addrs[NDIRECT]);
		ip->addrs[NDIRECT] = 0;
	}

	ip->size = 0;
	iupdate(ip);
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
int dirls(struct inode *dp)
{
	uint64 off, count;
	struct dirent de;

	if (dp->type != T_DIR)
		panic("dirlookup not DIR");

	count = 0;
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlookup read");
		if (de.inum == 0)
			continue;
		printf("%s\n", de.name);
		count++;
	}
	return count;
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
