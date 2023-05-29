#ifndef __FS_H__
#define __FS_H__

#include "types.h"
// On-disk file system format.
// Both the kernel and user programs use this header file.
//磁盘文件系统格式。
//内核程序和用户程序都使用这个头文件。

#define NFILE 100 // open files per system	每个系统打开文件的最大数量。
#define NINODE 50 // maximum number of active i-nodes	活动 i 节点的最大数量。
#define NDEV 10 // maximum major device number	主设备号的最大数量。
#define ROOTDEV 1 // device number of file system root disk	文件系统根目录磁盘的设备号。
#define MAXOPBLOCKS 10 // max # of blocks any FS op writes	任何文件系统操作写入的最大块数。
#define NBUF (MAXOPBLOCKS * 3) // size of disk block cache	磁盘块缓存的大小。
#define FSSIZE 1000 // size of file system in blocks	//文件系统的块大小
#define MAXPATH 128 // maximum file path name	//最大文件路径名

#define ROOTINO 1 // root i-number	根目录 i 节点号。
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
	uint magic; // Must be FSMAGIC
	uint size; // Size of file system image (blocks)
	uint nblocks; // Number of data blocks
	uint ninodes; // Number of inodes.
	uint inodestart; // Block number of first inode block
	uint bmapstart; // Block number of first free map block
};

#define FSMAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// File type
#define T_DIR 1 // Directory
#define T_FILE 2 // File

// LAB4: Keep it the same as dinode in os/fs.h after you change it
// On-disk inode structure
//LAB4: 修改后与os/fs.h中的dinode保持一致
//磁盘 inode 结构
// 储存磁盘 inode 信息，主要是文件类型和数据块的索引，其大小影响磁盘布局，不要乱改，可以用 pad
struct dinode {
	short type; // File type		文件类型。
	short pad[3];	//用于填充的数组，占用 3 个 short 类型的空间。
	uint size; // Size of file (bytes)		文件的大小（以字节为单位）。
	//包含直接数据块地址和一级间接块指针地址的数组。NDIRECT定义了直接数据块地址的数量，
	//而数组元素 addrs[NDIRECT] 则存储一级间接块的指针地址。
	uint addrs[NDIRECT + 1]; // Data block addresses
};

// Inodes per block.
#define IPB (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

// 目录对应的数据块的内容本质是 filename 到 file inode_num 的一个 map，这里为了简单，就存为一个 `dirent` 数组，查找的时候遍历对比
struct dirent {
	ushort inum;		//目录项对应文件的 i 节点号。
	char name[DIRSIZ];	//目录项对应文件的文件名，占用了 DIRSIZ 个字节的空间。
};

#endif //!__FS_H__
