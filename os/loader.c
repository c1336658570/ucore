#include "loader.h"
#include "defs.h"
#include "trap.h"

static uint64 app_num;
static uint64 *app_info_ptr;
extern char _app_num[], ekernel[];

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
	if ((uint64)ekernel >= BASE_ADDRESS) {
		panic("kernel too large...\n");
	}
	app_info_ptr = (uint64 *)_app_num;
	app_num = *app_info_ptr;
	app_info_ptr++;
}

//加载第 n 个用户应用程序
//用户程序加载的起始地址和终止地址：[BASE_ADDRESS + n * MAX_APP_SIZE, BASE_ADDRESS + (n+1) * MAX_APP_SIZE)
//每一个用户每个进程所使用的空间是 [0x80400000 + n*0x20000, 0x80400000 + (n+1)*0x20000)
int load_app(int n, uint64 *info)
{
	uint64 start = info[n], end = info[n + 1], length = end - start;
	memset((void *)BASE_ADDRESS + n * MAX_APP_SIZE, 0, MAX_APP_SIZE);
	memmove((void *)BASE_ADDRESS + n * MAX_APP_SIZE, (void *)start, length);
	return length;
}

//加载所有应用程序并初始化相应的 proc 结构。
//每一个用户每个进程所使用的空间是 [0x80400000 + i*0x20000, 0x80400000 + (i+1)*0x20000)
int run_all_app()
{
	//遍历每一个ieapp获取其放置位置
	for (int i = 0; i < app_num; ++i) {
		struct proc *p = allocproc();	//分配进程
		struct trapframe *trapframe = p->trapframe;	//分配trapframe结构体
		load_app(i, app_info_ptr);	//根据是第几个用户程序，将其加载到对应位置
		uint64 entry = BASE_ADDRESS + i * MAX_APP_SIZE;	//每一个应用程序的入口地址
		tracef("load app %d at %p", i, entry);
		trapframe->epc = entry;	//设置程序入口地址
		trapframe->sp = (uint64)p->ustack + USER_STACK_SIZE;	//设置应用程序用户栈地址
		p->state = RUNNABLE;	//将进程设置为可运行状态
		/*
		* LAB1: you may need to initialize your new fields of proc here
		*/
	}
	return 0;
}