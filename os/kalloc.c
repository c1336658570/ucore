#include "kalloc.h"
#include "defs.h"
#include "riscv.h"

extern char ekernel[];
//内存进行链式管理
struct linklist {
	struct linklist *next;
};

struct {
	struct linklist *freelist;
} kmem;	//保存空闲页的链表

void freerange(void *pa_start, void *pa_end)
{
	char *p;
	p = (char *)PGROUNDUP((uint64)pa_start);	//将pa_start向上对其的页面边界
	for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)	//只要未循环到物理内存末尾，就一直循环，初始化页
		kfree(p);
}

//我们在main函数中会执行kinit，它会初始化从ekernel到PHYSTOP的所有物理地址作为空闲的物理地址。
//freerange中调用的kfree函数以页为单位向对应内存中填入垃圾数据（全1），
//并把初始化好的一个页作为新的空闲listnode插入到链表首部。

//qemu已经规定了内核需要管理的内存范围，可以参考这里，具体来说，需要软件管理的内存为[0x80000000, 0x88000000)，
//其中，rustsbi使用了[0x80000000, 0x80200000)的范围，其余都是内核使用。
//ekernel为链接脚本定义的内核代码结束地址，PHYSTOP = 0x88000000
//ekernel-PHYSTOP都是空闲空间，用于给用户程序进行动态内存分配
void kinit()	//初始化整个页
{
	freerange(ekernel, (void *)PHYSTOP);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
//释放v指向的物理内存页，
//通常应该由
//调用 kalloc()。 （例外是当
//初始化分配器；参见上面的 kinit。）
void kfree(void *pa)
{
	struct linklist *l;
	if (((uint64)pa % PGSIZE) != 0 || (char *)pa < ekernel ||
	    (uint64)pa >= PHYSTOP)	//如果不是4k对齐或者小于ekernel或大于PHYSTOP就panic
		panic("kfree");
	// Fill with junk to catch dangling refs.
	memset(pa, 1, PGSIZE);	//以页为单位向对应内存中填入垃圾数据（全1）
	//将释放的页挂到空闲页链表上
	l = (struct linklist *)pa;
	l->next = kmem.freelist;
	kmem.freelist = l;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//分配一个 4096 字节的物理内存页。
//返回内核可以使用的指针。
//如果无法分配内存，则返回 0。
void *kalloc(void)
{
	struct linklist *l;
	l = kmem.freelist;
	if (l) {
		kmem.freelist = l->next;
		memset((char *)l, 5, PGSIZE); //填入垃圾数据
	}
	return (void *)l;
}