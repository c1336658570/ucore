//更加完成的文件定义

#ifndef FILE_H
#define FILE_H

#include "fs.h"
#include "proc.h"
#include "types.h"

#define PIPESIZE (512)
#define FILEPOOLSIZE (NPROC * FD_BUFFER_SIZE)

// in-memory copy of an inode,it can be used to quickly locate file entities on disk
//inode的内存拷贝, 可以快速定位磁盘上的文件实体。
// dinode 的内存缓存，为了方便，增加了 dev, inum, ref, valid 四项管理信息，大小无所谓，可以随便改。
struct inode {
	uint dev; // Device number		设备号，表示存储该inode所在文件系统的磁盘设备号。
	uint inum; // Inode number		inode号，表示该inode在磁盘上的编号。
	int ref; // Reference count		引用计数器，记录该inode当前被多少个进程或线程同时引用。
	int valid; // inode has been read from disk?		标识该inode是否已经从磁盘读取。如果已经读取，则为1，否则为0。
	short type; // copy of disk inode		文件类型，表示文件、目录、管道或者设备文件等的类型，是一个对应于磁盘inode的字段的副本。
	uint size;		//文件大小，表示文件的实际大小（以字节为单位）。
	uint addrs[NDIRECT + 1];	//：表示文件数据块的地址。其中addrs[0] ~ addrs[NDIRECT-1]存储直接寻址的数据块地址，addrs[NDIRECT]存储一级间接寻址块的地址。
	// LAB4: You may need to add link count here
	//link count：文件的硬链接数，表示有多少个目录项指向该inode。在Lab4中需要添加该成员来实现硬链接功能。
};

/*
inode是由操作系统统一控制的dinode在内存中的映射，但是每个进程在具体使用文件的时候，
除了需要考虑使用的是哪个inode对应的文件外，还需要根据对文件的使用情况来记录其它特性，
因此，在进程中我们使用file结构体来标识一个被进程使用的文件：
*/
// Defines a file in memory that provides information about the current use of the file and the corresponding inode location
//在内存中定义一个文件，提供有关文件当前使用情况和相应 inode 位置的信息
struct file {
	//FD_INODE表示file已经绑定了一个文件（可能是目录或普通文件），FD_NONE表示该file还没完成绑定,FD_STDIO用来做标准输入输出
	enum { FD_NONE = 0, FD_INODE, FD_STDIO } type;
	int ref; // reference count		记录了其引用次数
	char readable;		//readbale和writeble规定了进程对文件的读写权限；
	char writable;
	struct inode *ip; // FD_INODE		ip标识了file所对应的磁盘中的inode编号
	uint off;					//off即文件指针，用作记录文件读写时的偏移量。
};

//A few specific fd
enum {
	STDIN = 0,
	STDOUT = 1,
	STDERR = 2,
};
/*
我们采用预分配的方式来对file进行分配，每一个需要使用的file都要与filepool中的某一个file完成绑定。
file结构中，ref记录了其引用次数,type表示了文件的类型，在本章中我们主要使用FD_NONE和FD_INODE属性，
其中FD_INODE表示file已经绑定了一个文件（可能是目录或普通文件），FD_NONE表示该file还没完成绑定，
FD_STDIO用来做标准输入输出，这里不做讨论；readbale和writeble规定了进程对文件的读写权限；
ip标识了file所对应的磁盘中的inode编号，off即文件指针，用作记录文件读写时的偏移量。
*/
extern struct file filepool[FILEPOOLSIZE];

void fileclose(struct file *);
struct file *filealloc();
int fileopen(char *, uint64);
uint64 inodewrite(struct file *, uint64, uint64);
uint64 inoderead(struct file *, uint64, uint64);
struct file *stdio_init(int);
int show_all_files();

#endif // FILE_H