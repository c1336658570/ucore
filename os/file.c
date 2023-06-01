//更加完成的文件操作
#include "file.h"
#include "defs.h"
#include "fcntl.h"
#include "fs.h"
#include "proc.h"

//This is a system-level open file table that holds open files of all process.
//这是一个系统级的打开文件表，保存着所有进程的打开文件。
struct file filepool[FILEPOOLSIZE];

//Abstract the stdio into a file.
//将stdio抽象成一个文件。
struct file *stdio_init(int fd)
{
	struct file *f = filealloc();
	f->type = FD_STDIO;
	f->ref = 1;
	f->readable = (fd == STDIN || fd == STDERR);
	f->writable = (fd == STDOUT || fd == STDERR);
	return f;
}

//The operation performed on the system-level open file table entry after some process closes a file.
//在某个进程关闭文件后对系统级打开文件表条目执行的操作。
//进程关闭文件时，也要去filepool中放回：（注意需要根据ref来判断是否需要回收该file）
/*
文件读写结束后需要fclose释放掉其inode，同时释放OS中对应的file结构体和fd。
其实inode文件的关闭只需要调用iput就好了，iput的实现简单到让人感觉迷惑，就是inode引用计数减一。
诶？为什么没有计数为0就写回然后释放inode的操作？和buf的释放同理，
这里会等inode池满了之后自行被替换出去，重新读磁盘实在太太太太慢了。
对了，千万记得iput和iget数量相同，一定要一一对应
*/
void fileclose(struct file *f)
{
	if (f->ref < 1)
		panic("fileclose");
	if (--f->ref > 0) {
		return;
	}
	switch (f->type) {
	case FD_STDIO:
		// Do nothing
		break;
	case FD_INODE:
		iput(f->ip);
		break;
	default:
		panic("unknown file type %d\n", f->type);
	}

	f->off = 0;
	f->readable = 0;
	f->writable = 0;
	f->ref = 0;
	f->type = FD_NONE;
}

//Add a new system-level table entry for the open file table
//为打开的文件表添加一个新的系统级表项
//分配文件时，我们从filepool中寻找还没有被分配的file进行分配
struct file *filealloc()
{
	for (int i = 0; i < FILEPOOLSIZE; ++i) {
		if (filepool[i].ref == 0) {
			filepool[i].ref = 1;
			return &filepool[i];
		}
	}
	return 0;
}

//Show names of all files in the root_dir.
int show_all_files()
{
	return dirls(root_dir());
}

//Create a new empty file based on path and type and return its inode;
//if the file under the path exists, return its inode;
//returns 0 if the type of file to be created is not T_file
//根据指定的路径和文件类型创建一个新的空文件，并返回该文件的 i 节点（inode）；
//如果该路径下已经存在相应的文件，则返回该文件的 i 节点；
//如果待创建的文件类型不是 T_file，则返回 0。
static struct inode *create(char *path, short type)
{
	struct inode *ip, *dp;
	//记住这一步root_inode是打开的，所以需要关闭。
	dp = root_dir(); //Remember that the root_inode is open in this step,so it needs closing then.
	ivalid(dp);
	if ((ip = dirlookup(dp, path, 0)) != 0) {
		warnf("create a exist file\n");
		iput(dp); //Close the root_inode
		ivalid(ip);
		if (type == T_FILE && ip->type == T_FILE)
			return ip;
		iput(ip);
		return 0;
	}
	//创建一个文件,首先分配一个空闲的disk inode, 绑定内存inode之后返回
	if ((ip = ialloc(dp->dev, type)) == 0)
		panic("create: ialloc");

	tracef("create dinode and inode type = %d\n", type);

	// 注意ialloc不会执行实际读取，必须有ivalid
	ivalid(ip);
	iupdate(ip);
	//在根目录创建一个dirent指向刚才创建的inode
	if (dirlink(dp, path, ip->inum) < 0)
		panic("create: dirlink");
	//dp不用了，iput就是释放内存inode，和iget正好相反。
	iput(dp);
	return ip;
}

//A process creates or opens a file according to its path, returning the file descriptor of the created or opened file.
//If omode is O_CREATE, create a new file
//if omode if the others,open a created file.
//进程根据文件路径创建或打开文件，并返回已创建或已打开文件的文件描述符。
//如果 omode 是 O_CREATE（创建选项），则创建一个新文件。
//如果 omode 是其他选项，则打开一个已创建的文件。
int fileopen(char *path, uint64 omode)
{
	int fd;
	struct file *f;
	struct inode *ip;
	/*
	fileopen 还可能会导致文件 truncate，也就是截断，具体做法是舍弃全部现有内容，
	释放inode所有 data block 并添加到 free bitmap 里。这也是目前 nfs 中唯一的文件变短方式。
	比较复杂的就是使用fileopen以创建的方式打开一个文件。
	*/
	if (omode & O_CREATE) {
		ip = create(path, T_FILE);
		if (ip == 0) {
			return -1;
		}
	} else {
		if ((ip = namei(path)) == 0) {	//通过namei查找inode节点
			return -1;
		}
		ivalid(ip);
	}
	if (ip->type != T_FILE)
		panic("unsupported file inode type\n");
	if ((f = filealloc()) == 0 ||
	    (fd = fdalloc(f)) <
		    0) { //Assign a system-level table entry to a newly created or opened file
		//and then create a file descriptor that points to it
		if (f)
			fileclose(f);
		iput(ip);
		return -1;
	}
	// only support FD_INODE
	f->type = FD_INODE;
	f->off = 0;
	f->ip = ip;
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
	if ((omode & O_TRUNC) && ip->type == T_FILE) {
		itrunc(ip);
	}
	return fd;
}

// Write data to inode.
//向inode写入数据
uint64 inodewrite(struct file *f, uint64 va, uint64 len)
{
	int r;
	ivalid(f->ip);
	if ((r = writei(f->ip, 1, va, f->off, len)) > 0)
		f->off += r;
	return r;
}

//Read data from inode.
//从inode读取数据。
uint64 inoderead(struct file *f, uint64 va, uint64 len)
{
	int r;
	ivalid(f->ip);
	if ((r = readi(f->ip, 1, va, f->off, len)) > 0)
		f->off += r;
	return r;
}