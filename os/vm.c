/*
用户在启用了虚拟内存之后，用户 syscall 给出的指针是不能直接用的，因为与内核的映射不一样，
会读取错误的物理地址，使用指针必须通过 useraddr 转化，当然，更加推荐的是 copyin/out 接口，
否则很可能损坏内存数据，同时，copyin/out 接口处理了虚存跨页的情况，useraddr 则需要手动判断并处理。
*/
#include "vm.h"
#include "defs.h"
#include "riscv.h"

pagetable_t kernel_pagetable;

extern char e_text[]; //内核代码结尾
extern char trampoline[];

// Make a direct-map page table for the kernel.
//为内核制作一个直接映射页表。
//开启页表之后，内核也需要进行映射处理。但是我们这里可以直接进行一一映射，
//也就是va经过MMU转换得到的pa就是va本身（但是转换过程还是会执行！）。
pagetable_t kvmmake(void)
{
	pagetable_t kpgtbl;
	kpgtbl = (pagetable_t)kalloc();
	memset(kpgtbl, 0, PGSIZE);
	// map kernel text executable and read-only.
	//映射内核文本可执行且只读。
	//虚拟地址和物理地址都是KERNBASE -> (uint64)e_text - KERNBASE
	kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)e_text - KERNBASE,
	       PTE_R | PTE_X);
	// map kernel data and the physical RAM we'll make use of.
	//映射内核数据和我们将使用的物理 RAM。
	//虚拟地址和物理地址都是(uint64)e_text -> PHYSTOP - (uint64)e_text
	kvmmap(kpgtbl, (uint64)e_text, (uint64)e_text, PHYSTOP - (uint64)e_text,
	       PTE_R | PTE_W);
	//映射trampoline
	kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
	return kpgtbl;
}

// Initialize the one kernel_pagetable
// Switch h/w page table register to the kernel's page table,
// and enable paging.
void kvm_init(void)
{
	kernel_pagetable = kvmmake();
	w_satp(MAKE_SATP(kernel_pagetable));
	sfence_vma();
	infof("enable pageing at %p", r_satp());
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
//返回PTE在页表pagetable中的地址
//对应于虚拟地址va。如果alloc！=0，
//创建任何需要的页表页面。
//
//risc-v Sv39方案有三级页表
//页。页表页包含 512 个 64 位 PTE。
//一个 64 位的虚拟地址分为五个字段：
//39..63 --必须为零。
//30..38 --9 位二级索引。
//21..29 --9 位一级索引。
//12..20 --9 位 0 级索引。
//0..11 --页内的 12 位字节偏移量。

//模拟了CPU进行MMU的过程
//三个参数分别是页表，待转换的虚拟地址va，以及如果没有对应的物理地址时是否分配物理地址。
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc)
{
	if (va >= MAXVA)
		panic("walk");

	//循环取出PPN[2],PPN[1]
	for (int level = 2; level > 0; level--) {
		pte_t *pte = &pagetable[PX(
			level,
			va)]; //调用PX时level=2,取出PPN[2]，level=1，取出PPN[1]，然后通过访问pagetable数组，取出页表项
		if (*pte &
		    PTE_V) { //如果该页表项有效，就取出高44位，低10位是一些标志位如X，R，W，D，G等
			pagetable = (pagetable_t)PTE2PA(
				*pte); //获取下一级页表的起始物理地址了
		} else { //该页表项无效，就分配一个页。
			if (!alloc || (pagetable = (pde_t *)kalloc()) == 0)
				return 0;
			memset(pagetable, 0, PGSIZE);
			*pte = PA2PTE(pagetable) |
			       PTE_V; //让*pte即该级页表的一个页表项指向新分配的页
		}
	}
	return &pagetable[PX(
		0,
		va)]; //返回最低一级的页表项，可以根据该页表项的高位和虚拟地址的12位偏移量共同组成物理地址
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
//查找虚拟地址，返回物理地址，
//如果未映射，则为 0。
//只能用于查找用户页面。
uint64 walkaddr(pagetable_t pagetable, uint64 va)
{
	pte_t *pte;
	uint64 pa;

	if (va >= MAXVA) //超出最大虚拟地址
		return 0;

	pte = walk(pagetable, va, 0);
	if (pte == 0)
		return 0;
	if ((*pte & PTE_V) == 0) //无效
		return 0;
	if ((*pte & PTE_U) == 0) //非用户页面
		return 0;
	pa = PTE2PA(*pte); //获取虚拟地址对应的页的物理地址
	return pa;
}

// Look up a virtual address, return the physical address,
//查找虚拟地址，返回物理地址
//未处理虚存跨页情况
uint64 useraddr(pagetable_t pagetable, uint64 va)
{
	uint64 page = walkaddr(pagetable, va);
	if (page == 0)
		return 0;
	return page | (va & 0xFFFULL); //获取物理地址
}

// Add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
//添加一个映射到内核页表。
//仅在引导时使用。
//不刷新 TLB 或启用分页。
//参数perm为一些控制位（权限等如RWX）
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
	if (mappages(kpgtbl, va, sz, pa, perm) != 0)
		panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
//针对从虚拟地址 va 开始的内存区域，创建对应的页表项（PTE），
//使其映射到从物理地址 pa 开始的内存区域。虚拟地址 va 和内存大小 size 不一定是对齐的。
//如果walk()无法分配需要的页表页，则返回-1，否则返回0表示成功。
//参数perm为一些控制位（权限等如RWX）
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
	uint64 a, last;
	pte_t *pte;

	a = PGROUNDDOWN(va); //虚拟地址低12位清零（获取第一页的虚拟地址）
	last = PGROUNDDOWN(
		va + size -
		1); //要转换的最后一个字节的虚拟地址低12位清零（即获取最后一页的虚拟地址）
	for (;;) {
		if ((pte = walk(pagetable, a, 1)) ==
		    0) //获取最低一级的页表项（PTE）
			return -1;
		if (*pte & PTE_V) { //有效就打印错误信息，并返回
			errorf("remap");
			return -1;
		}
		*pte = PA2PTE(pa) | perm | PTE_V; //物理地址到PTE（页表项）
		if (a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
//删除从 va 开始的 n 页映射。必须是
//页面对齐。映射必须存在。
//可选择释放物理内存。
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
	uint64 a;
	pte_t *pte;

	if ((va % PGSIZE) != 0) //不是对其的就panic
		panic("uvmunmap: not aligned");

	for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
		if ((pte = walk(pagetable, a, 0)) == 0) //获取a的最低一级的PTE
			continue;
		if ((*pte & PTE_V) != 0) { //PTE有效
			if (PTE_FLAGS(*pte) ==
			    PTE_V) //低12位==PTE_V，即只有PTE_V为1,其他位都为0，就panic
				panic("uvmunmap: not a leaf");
			if (do_free) { //do_free为1
				uint64 pa = PTE2PA(*pte); //将PTE转为物理地址
				kfree((void *)pa); //释放物理地址
			}
		}
		*pte = 0; //删除PTE
	}
}

// create an empty user page table.
// returns 0 if out of memory.
//创建一个空的用户页表。
//如果内存不足则返回 0。
pagetable_t uvmcreate()
{
	pagetable_t pagetable;
	pagetable = (pagetable_t)kalloc();
	if (pagetable == 0) {
		errorf("uvmcreate: kalloc error");
		return 0;
	}
	memset(pagetable, 0, PGSIZE);
	if (mappages(pagetable, TRAMPOLINE, PAGE_SIZE, (uint64)trampoline,
		     PTE_R | PTE_X) < 0) {
		kfree(pagetable);
		errorf("uvmcreate: mappages error");
		return 0;
	}
	return pagetable;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
//递归释放页表页面。
//必须已删除所有叶映射。
void freewalk(pagetable_t pagetable)
{
	// there are 2^9 = 512 PTEs in a page table.
	//页表中有 2^9 = 512 个 PTE。
	for (int i = 0; i < 512; i++) {
		pte_t pte = pagetable[i]; //取PTE
		if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
			// this PTE points to a lower-level page table.
			//这个 PTE 指向一个较低级别的页表。
			uint64 child = PTE2PA(pte); //PTE转PA
			freewalk((pagetable_t)child); //释放
			pagetable[i] = 0; //将页表项设置为0
		} else if (pte & PTE_V) { //有效且RWX都不为1就panic
			panic("freewalk: leaf");
		}
	}
	kfree((void *)pagetable);
}

/**
 * @brief Free user memory pages, then free page-table pages.
 *
 * @param max_page The max vaddr of user-space.
 */
/*
*@brief 释放用户内存页面，然后释放页表页面。
*
*@param max_page 用户空间的最大vaddr。
*/
void uvmfree(pagetable_t pagetable, uint64 max_page)
{
	if (max_page > 0)
		uvmunmap(
			pagetable, 0, max_page,
			1); //释放虚拟地址从0 -> max_page之间的所有虚拟内存所对应的物理内存，并清除PTE
	freewalk(pagetable); //递归释放页表页面
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
//从内核复制到用户。
//将 len 个字节从 src 复制到给定页表中的虚拟地址 dstva。
//成功返回 0，错误返回 -1。
//copyout 可以向页表中写东西，后续用于 sys_read，也就是给用户传数据
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
	uint64 n, va0, pa0;

	while (len > 0) {
		va0 = PGROUNDDOWN(dstva); //低12位清零
		pa0 = walkaddr(pagetable, va0); ////获取该虚拟页对应的物理页地址
		if (pa0 == 0)
			return -1;
		n = PGSIZE -
		    (dstva - va0); //通过PGSIZE-页内偏移获取页剩余字节数
		if (n > len)
			n = len;
		memmove((void *)(pa0 + (dstva - va0)), src, n);

		len -= n;
		src += n;
		dstva = va0 + PGSIZE; //进入下一页（处理虚存跨页的情况）
	}
	return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
//从用户复制到内核。
//从给定页表中的虚拟地址 srcva 复制 len 个字节到 dst。
//成功返回 0，错误返回 -1。
//copyin 用户接受用户的 buffer，也就是从用户哪里读数据。
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
	uint64 n, va0, pa0;

	while (len > 0) {
		va0 = PGROUNDDOWN(srcva); //低12位清零
		pa0 = walkaddr(pagetable, va0); ////获取该虚拟页对应的物理页地址
		if (pa0 == 0)
			return -1;
		n = PGSIZE -
		    (srcva - va0); //通过PGSIZE-页内偏移获取页剩余字节数
		if (n >
		    len) //如果剩余字节比要写入内核的字节数多就让n=要写入内核的字节数
			n = len;
		memmove(dst, (void *)(pa0 + (srcva - va0)), n);

		len -= n;
		dst += n;
		srcva = va0 + PGSIZE; //进入下一页（处理虚存跨页的情况）
	}
	return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
//将以 null 结尾的字符串从用户复制到内核。
//从给定页表中的虚拟地址 srcva 复制字节到 dst，
//直到 '\0'，或最大值。
//成功返回 0，错误返回 -1。
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
	uint64 n, va0, pa0;
	int got_null = 0, len = 0;

	while (got_null == 0 && max > 0) {
		va0 = PGROUNDDOWN(srcva); //低12位清0
		pa0 = walkaddr(pagetable, va0); //获取该虚拟页对应的物理页地址
		if (pa0 == 0)
			return -1;
		n = PGSIZE -
		    (srcva - va0); //通过PGSIZE-页内偏移获取页剩余字节数
		if (n >
		    max) //如果剩余字节比要写入内核的字节数多就让n=要写入内核的字节数
			n = max;

		char *p = (char *)(pa0 + (srcva - va0)); //获取源字节的物理地址
		while (n > 0) {
			if (*p == '\0') {
				*dst = '\0';
				got_null = 1;
				break;
			} else {
				*dst = *p;
			}
			--n;
			--max;
			p++;
			dst++;
			len++;
		}

		srcva = va0 + PGSIZE; //进入下一页（处理虚存跨页的情况）
	}
	return len;
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
//分配页表项和物理内存以将进程从旧尺寸 oldsz 扩展到新尺寸 newsz，
//新尺寸不需要对齐到页边界。函数返回新的进程大小，如果出现错误，则返回0。
//参数oldsz，和newsz不是大小，而是旧的内存的地址和新的内存的虚拟地址。
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
	char *mem;
	uint64 a;

	if (newsz < oldsz)
		return oldsz;

	oldsz = PGROUNDUP(oldsz);		//将oldsz向上对齐到页面边界上。
	for (a = oldsz; a < newsz; a += PGSIZE) {
		mem = kalloc();	//分配页
		if (mem == 0) {	//分配失败
			uvmdealloc(pagetable, a, oldsz);	//将之前分配的释放掉，第一次循环就分配失败进入该函数什么都不做
			return 0;
		}
		memset(mem, 0, PGSIZE);	//页初始化为0
		if (mappages(pagetable, a, PGSIZE, (uint64)mem,
			     PTE_R | PTE_U | xperm) != 0) {	//虚拟地址从a开始PGSIZE个字节映射到mem，映射失败就释放内存，并删除页表项。
			kfree(mem);
			uvmdealloc(pagetable, a, oldsz);
			return 0;
		}
	}
	return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
//回收内存页，以将进程的大小从旧尺寸 oldsz 调整为新尺寸 newsz。
//旧尺寸和新尺寸不需要对齐到页边界，newsz 必需要小于 oldsz。oldsz 可以大于实际进程大小。
//函数返回新的进程大小。
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
	if (newsz >= oldsz)
		return oldsz;

	if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {	//newsz和oldsz向上对齐到页面边界上。对其后newsz < oldsz
		int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;	//求需要释放的页面数量
		uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);	//删除从newsz开始的npages页映射，并释放物理内存。
	}

	return newsz;
}
