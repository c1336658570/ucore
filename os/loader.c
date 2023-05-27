#include "loader.h"
#include "defs.h"
#include "trap.h"

static int app_num;
static uint64 *app_info_ptr;
extern char _app_num[];

// Count finished programs. If all apps exited, shutdown.
int finished()
{
	static int fin = 0;
	if (++fin >= app_num)
		panic("all apps over");
	return 0;
}

// Get user progs' infomation through pre-defined symbol in `link_app.S`
void loader_init()
{
	app_info_ptr = (uint64 *)_app_num;
	app_num = *app_info_ptr;
	app_info_ptr++;
}

pagetable_t bin_loader(uint64 start, uint64 end, struct proc *p)
{
	//pg代表根页表地址，根页表大小恰好为1个页
	//创建一个空的用户页表。
	pagetable_t pg = uvmcreate();	
	// 映射 trapframe（中断帧），注意这里的权限!
	//将虚拟地址TRAPFRAME，大小为PGSIZE的地址映射到物理地址p->trapframe，大小为PGSIZE
	if (mappages(pg, TRAPFRAME, PGSIZE, (uint64)p->trapframe, 
			PTE_R | PTE_W) < 0) {
		panic("mappages fail");
	}

	// 接下来映射用户实际地址空间，也就是把 physics address [start, end)
  // 映射到虚拟地址 [BASE_ADDRESS, BASE_ADDRESS + length)

	// riscv 指令有对齐要求，同时,如果不对齐直接映射的话会把部分内核地址映射到用户态，很不安全
	// ch5我们就不需要这个限制了。
	if (!PGALIGNED(start)) {	//判断低12位是否为0（4k对齐），不是0就panic
		panic("user program not aligned, start = %p", start);
	}
	if (!PGALIGNED(end)) {		//判断是否4k对齐
		// Fix in ch5
		warnf("Some kernel data maybe mapped to user, start = %p, end = %p",
		      start, end);
	}
	end = PGROUNDUP(end);	//将end向上对齐到页面边界
	uint64 length = end - start;	//获取要映射的长度
	//针对从虚拟地址 BASE_ADDRESS 开始的内存区域，创建对应的页表项（PTE），
	//使其映射到从物理地址 start 开始的内存区域。
	//将从BASE_ADDRESS开始的长度为length的虚拟地址映射到start，长度为length
	if (mappages(pg, BASE_ADDRESS, length, start, 
			PTE_U | PTE_R | PTE_W | PTE_X) != 0) {	
		panic("mappages fail");
	}
	p->pagetable = pg;	//让进程的pagetable和pg指向同一块内存
	// 接下来 map user stack， 注意这里的虚存选择，想想为何要这样？
	uint64 ustack_bottom_vaddr = BASE_ADDRESS + length + PAGE_SIZE;	//ustack_bottom_vaddr指用户栈的底部虚拟地址
	if (USTACK_SIZE != PAGE_SIZE) {
		// Fix in ch5
		panic("Unsupported");
	}
	//用户栈映射。针对从虚拟地址 ustack_bottom_vaddr 开始的内存区域，创建对应的页表项（PTE），
	//使其映射到从物理地址 kalloc() 开始的内存区域。
	//将ustack_bottom_vaddr开始大小为USTACK_SIZE的虚拟地址映射到kalloc分配的4k的内存
	mappages(pg, ustack_bottom_vaddr, USTACK_SIZE, (uint64)kalloc(),
		 PTE_U | PTE_R | PTE_W | PTE_X);
	p->ustack = ustack_bottom_vaddr;	//修改用户栈底部指针
	// 设置trapframe
	p->trapframe->epc = BASE_ADDRESS;	//修改epc指针，当
	p->trapframe->sp = p->ustack + USTACK_SIZE;	//修改栈顶指针
	 // exit 的时候会 uvmunmap 页表中 [BASE_ADDRESS, max_page * PAGE_SIZE) 的页
	p->max_page = PGROUNDUP(p->ustack + USTACK_SIZE - 1) / PAGE_SIZE;	//最大页号
	//此时堆还未分配内存，所以堆底bottom和堆顶brk指向同一个位置
	p->program_brk = p->ustack + USTACK_SIZE;
  p->heap_bottom = p->ustack + USTACK_SIZE;
	return pg;
}

// load all apps and init the corresponding `proc` structure.
int run_all_app()
{
	for (int i = 0; i < app_num; ++i) {
		struct proc *p = allocproc();
		tracef("load app %d", i);
		bin_loader(app_info_ptr[i], app_info_ptr[i + 1], p);
		p->state = RUNNABLE;
		/*
		* LAB1: you may need to initialize your new fields of proc here
		*/
	}
	return 0;
}
