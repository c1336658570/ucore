#include "loader.h"
#include "defs.h"
#include "trap.h"

static int app_num;
static uint64 *app_info_ptr;
extern char _app_num[], _app_names[], INIT_PROC[];
char names[MAX_APP_NUM][MAX_STR_LEN];

// Get user progs' infomation through pre-defined symbol in `link_app.S`
void loader_init()
{
	char *s;
	app_info_ptr = (uint64 *)_app_num;
	app_num = *app_info_ptr;
	app_info_ptr++;
	s = _app_names;
	printf("app list:\n");
	for (int i = 0; i < app_num; ++i) {
		int len = strlen(s);
		strncpy(names[i], (const char *)s, len);
		s += len + 1;
		printf("%s\n", names[i]);
	}
}

//通过可执行程序名返回其是第几个
int get_id_by_name(char *name)
{
	for (int i = 0; i < app_num; ++i) {
		if (strncmp(name, names[i], 100) == 0)
			return i;
	}
	warnf("Cannot find such app %s", name);
	return -1;
}

//对于用户栈、trapframe、trampoline 的映射没有变化
//在proc.c中的allocproc函数中完成了对trapframe、trampoline的映射，alllocproc会调用uvmcreate，uvmcreate会对trapframe和trampoline进行映射
int bin_loader(uint64 start, uint64 end, struct proc *p)
{
	if (p == NULL || p->state == UNUSED)
		panic("...");
	void *page;
	// 注意现在我们不要求对其了，代码的核心逻辑还是把 [start, end)
	// 映射到虚拟内存的 [BASE_ADDRESS, BASE_ADDRESS + length)
	uint64 pa_start = PGROUNDDOWN(start);	//清空低12位，向下对齐到4k
	uint64 pa_end = PGROUNDUP(end);	//加4096-1，然后清空低12位，向上对齐到页面边界
	uint64 length = pa_end - pa_start;	//需要加载并映射的程序长度
	uint64 va_start = BASE_ADDRESS;			//虚拟起始地址
	uint64 va_end = BASE_ADDRESS + length;	//虚拟结束地址
	/*
	循环：对于.bin 的每一页，都申请一个新页并进行内容拷贝，最后建立这一页的映射。
	为什么要拷贝呢？想想lab4我们是怎么干的，直接把虚存和物理内存映射就好了，
	根本没有拷贝。那么，拷贝是为了什么呢？其实，按照lab4的做法，
	程序运行之后就会修改仅有一份的程序”原像”，你会发现，lab4的程序都是一次性的，
	如果第二次执行，会发现.data 和.bss段数据都被上一次执行改掉了，不是初始化的状态。
	但是lab4的时候，每个程序最多执行一次，所以这么做是可以的。但在lab5所有程序都可能被无数次的执行，
	我们就必须对“程序原像”做保护，在“原像”的拷贝上运行程序了。
	*/
	for (uint64 va = va_start, pa = pa_start; pa < pa_end;
	     va += PGSIZE, pa += PGSIZE) {
		page = kalloc();	//分配页面
		if (page == 0) {
			panic("...");
		}
		// 这里我们不会直接映射，而是新分配一个页面，然后使用 memmove 进行拷贝
    // 这样就不会有对其的问题了，但为何这么做其实有更深层的原因。
		memmove(page, (const void *)pa, PGSIZE);	//将pa所指内存复制到新分配的页
		// 这个 if 就是为了防止 start end 不对其导致拷贝了多余的内核数据
		// 我们需要手动把它们清空
		//在第一次进入循环时pa可能小于start，因为第一次循环pa是等于pa_start的，而pa_start是将start向下对齐到页面边界
		if (pa < start) {	
			memset(page, 0, start - va);	//个人觉得此处va应该改为pa
		} else if (pa + PAGE_SIZE > end) {	
			memset(page + (end - pa), 0, PAGE_SIZE - (end - pa));//将end到pa+PAGE_SIZE初始化为0
		}
		//进行映射
		if (mappages(p->pagetable, va, PGSIZE, (uint64)page,
			     PTE_U | PTE_R | PTE_W | PTE_X) != 0)
			panic("...");
	}
	//映射u态栈
	p->ustack = va_end + PAGE_SIZE;
	for (uint64 va = p->ustack; va < p->ustack + USTACK_SIZE;
	     va += PGSIZE) {
		page = kalloc();
		if (page == 0) {
			panic("...");
		}
		memset(page, 0, PGSIZE);
		if (mappages(p->pagetable, va, PGSIZE, (uint64)page,
			     PTE_U | PTE_R | PTE_W) != 0)
			panic("...");
	}
	//修改sp，epc，max_page等
	p->trapframe->sp = p->ustack + USTACK_SIZE;
	p->trapframe->epc = va_start;
	p->max_page = PGROUNDUP(p->ustack + USTACK_SIZE - 1) / PAGE_SIZE;
	p->program_brk = p->ustack + USTACK_SIZE;
        p->heap_bottom = p->ustack + USTACK_SIZE;
	p->state = RUNNABLE;
	return 0;
}

int loader(int app_id, struct proc *p)
{
	return bin_loader(app_info_ptr[app_id], app_info_ptr[app_id + 1], p);
}

// load all apps and init the corresponding `proc` structure.
int load_init_app()
{
	int id = get_id_by_name(INIT_PROC);	//INIT_PROC保存的是usershell，可以通过查看通过pack.py生成的link_app.S找到
	if (id < 0)
		panic("Cannpt find INIT_PROC %s", INIT_PROC);
	//在proc.c中的alllocproc函数会调用uvmcreate，uvmcreate会对trapframe和trampoline进行映射
	struct proc *p = allocproc();
	if (p == NULL) {
		panic("allocproc\n");
	}
	debugf("load init proc %s", INIT_PROC);
	loader(id, p);	//加载
	add_task(p);	//添加到就绪队列
	return 0;
}
