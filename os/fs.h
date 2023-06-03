/*
// 基本信息：块大小 BSIZE = 1024B，总容量 FSSIZE = 1000 个 block = 1000 * 1024 B。
// Layout: // 0号块留待后续拓展，可以忽略。superblock 固定为 1 号块，size 固定为一个块。
// 其后是储存 inode 的若干个块，占用块数 = inode 上限 / 每个块上可以容纳的 inode 数量，
// 其中 inode 上限固定为 200，每个块的容量 = BSIZE / sizeof(struct disk_inode) 
// 再之后是数据块相关内容，包含一个 储存空闲块位置的 bitmap 和 实际的数据块，bitmap 块 
// 数量固定为 NBITMAP = FSSIZE / (BSIZE * 8) + 1 = 1000 / 8 + 1 = 126 块。 
// [ boot block | sb block | inode blocks | free bit map | data blocks ]
*/

#ifndef __FS_H__
#define __FS_H__

#include "types.h"
// On-disk file system format.
// Both the kernel and user programs use this header file.
//磁盘文件系统格式。
//内核程序和用户程序都使用这个头文件。

#define NFILE 100 // open files per system	每个系统最多可以打开的文件数。
#define NINODE 50 // maximum number of active i-nodes	活动 i 节点的最大数量。
#define NDEV 10 // maximum major device number	主设备号的最大数量。
#define ROOTDEV 1 // device number of file system root disk	文件系统根目录磁盘的设备号。
#define MAXOPBLOCKS 10 // max # of blocks any FS op writes	任何文件系统操作写入的最大块数。
#define NBUF (MAXOPBLOCKS * 3) // size of disk block cache	磁盘块缓存的大小，以块数计。
#define FSSIZE 1000 // size of file system in blocks	//文件系统的大小（块数）
#define MAXPATH 128 // maximum file path name	//最大文件路径名

#define ROOTINO 1 // root i-number	文件系统根目录的i节点号。
#define BSIZE 1024 // block size		磁盘块大小。

// Disk layout:
// [ boot block | super block | inode blocks | free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
//磁盘布局：
//[引导块|超级块|索引节点块|空闲位图|数据块]
//mkfs 计算超级块并构建初始文件系统。超级块描述磁盘布局：
// 超级块位置固定，用来指示文件系统的一些元数据，这里最重要的是 inodestart 和 bmapstart
struct superblock {
	uint magic; // Must be FSMAGIC	文件系统魔数，必须为 FSMAGIC，以确认文件系统的有效性。
	uint size; // Size of file system image (blocks)	 文件系统镜像的大小（以块为单位）。
	uint nblocks; // Number of data blocks		数据块的数量，表示文件系统可用于存储数据的块数。
	uint ninodes; // Number of inodes.				 i 节点的数量，表示文件系统可以支持的最大文件数或目录数。
	uint inodestart; // Block number of first inode block		第一个inode块的块号，表示inode块的起始位置。
	uint bmapstart; // Block number of first free map block	第一个空闲块位图块的块号，表示空闲块位图的起始位置。
};

#define FSMAGIC 0x10203040		//文件系统魔数，用于标识文件系统类型。

#define NDIRECT 12						//直接地址数，指定i节点中能够存储指向数据块地址的直接指针数量。
////一次间接块地址数，指定 i 节点中能够存储指向数据块地址的一次间接指针数量；
//每个一次间接块中可存储 (BSIZE/sizeof(uint)) 个直接块地址，uint 为无符号整型。
#define NINDIRECT (BSIZE / sizeof(uint))	
#define MAXFILE (NDIRECT + NINDIRECT)	//指定 i 节点中直接块和一次间接块中能够存储的数据块地址数之和。

// File type
#define T_DIR 1 // Directory		目录文件类型。
#define T_FILE 2 // File				普通文件类型。

// On-disk inode structure
//磁盘 inode 结构
// 储存磁盘 inode 信息，主要是文件类型和数据块的索引，其大小影响磁盘布局，不要乱改，可以用 pad
struct dinode {
	short type; // File type		文件类型。
	short pad[3];	//用于填充的数组，占用3个short类型的空间。
	//在LAB4环节中，可以将这个数组的大小减小，并将一个部分作为链接计数器（link count）。
	// LAB4: you can reduce size of pad array and add link count below,
	//       or you can just regard a pad as link count.
	//       But keep in mind that you'd better keep sizeof(dinode) unchanged
	uint size; // Size of file (bytes)		文件的大小（以字节为单位）。
	//包含直接数据块地址和一级间接块指针地址的数组。
	//NDIRECT 定义了直接数据块地址的数量，而数组元素 addrs[NDIRECT] 则存储一级间接块的指针地址。
	uint addrs[NDIRECT + 1]; // Data block addresses	//数据块地址	包含实际保存对应文件/目录数据的数据块（位于最后的数据块区域中）的索引信息，从而能够找到文件/目录的数据被保存在磁盘的哪些块中。
};

// Inodes per block.
//每个磁盘块中dinode数的最大值
#define IPB (BSIZE / sizeof(struct dinode))		//每块中dinode数。

// Block containing inode i
//存储 inode i 内容的磁盘块
//通过i/IPB计算出当前inode节点位于第几个inode磁盘块，再加上初始inode磁盘块的块号获得但前i节点的磁盘号
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)	//返回i节点所在块号，sb为超级块。

// Bitmap bits per block
//位图(bitmap)每个磁盘块包含的比特数。
#define BPB (BSIZE * 8)		//每块中位图位数。

// Block of free map containing bit for block b
//包含磁盘块 b 分配信息所在的空闲块位图的磁盘块。
//通过磁盘块块号b除每个磁盘块可以包含的位图数，获取当前磁盘块的位图，在第几个位图块，再加上位图块起始块号，获取当前磁盘块的位图块
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)	//返回数据块b所在磁盘块的位图号，sb为超级块。

// Directory is a file containing a sequence of dirent structures.
//目录是一个包含一系列 dirent 结构的文件。
#define DIRSIZ 14		//目录结构体dirent中的最大文件名长度。

//目录对应的数据块的内容本质是 filename 到 file inode_num 的一个 map，这里为了简单，就存为一个 `dirent` 数组，查找的时候遍历对比
struct dirent {
	ushort inum;		//目录项对应文件的inode号。
	char name[DIRSIZ];	//目录项对应文件的文件名，占用了DIRSIZ个字节的空间，未使用的部分将被填充为 '\0'。
};

// file.h
struct inode;

void fsinit();
int dirlink(struct inode *, char *, uint);
struct inode *dirlookup(struct inode *, char *, uint *);
struct inode *ialloc(uint, short);
struct inode *idup(struct inode *);
void iinit();
void ivalid(struct inode *);
void iput(struct inode *);
void iunlock(struct inode *);
void iunlockput(struct inode *);
void iupdate(struct inode *);
struct inode *namei(char *);
struct inode *root_dir();
int readi(struct inode *, int, uint64, uint, uint);
int writei(struct inode *, int, uint64, uint, uint);
void itrunc(struct inode *);
int dirls(struct inode *);
#endif //!__FS_H__
