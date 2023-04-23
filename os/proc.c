#include "proc.h"
#include "defs.h"
#include "loader.h"
#include "trap.h"

struct proc pool[NPROC];	//全局进程池
char kstack[NPROC][PAGE_SIZE];	//内核页表

// 由于还有没内存管理机制，静态分配一些进程资源
//用户栈虚拟地址（用户页表），内核栈虚拟地址（内核页表），4096字节对齐
__attribute__((aligned(4096))) char ustack[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char trapframe[NPROC][PAGE_SIZE];

extern char boot_stack_top[];
struct proc *current_proc;	//指向当前正在执行的进程
struct proc idle;		//boot进程（执行初始化的进程）

//返回当前进程id
int threadid()
{
	return curr_proc()->pid;
}

//返回当前进程控制块
struct proc *curr_proc()
{
	return current_proc;
}

//进程模块初始化函数
//在启动时初始化 proc 表
void proc_init(void)
{
	struct proc *p;
	for (p = pool; p < &pool[NPROC]; p++) {
		p->state = UNUSED;	//将所有进程设置为未使用状态
		p->kstack = (uint64)kstack[p - pool];	//p-pool是两个指针相减，代表俩指针之间该结构的个数，初始化内核栈虚拟地址
		p->ustack = (uint64)ustack[p - pool];	//初始化用户栈虚拟地址
		p->trapframe = (struct trapframe *)trapframe[p - pool];	//初始化进程中断帧
		/*
		* LAB1: you may need to initialize your new fields of proc here
		*/
	}
	idle.kstack = (uint64)boot_stack_top;	//设置idel进程内核栈顶
	idle.pid = 0;	//设置idle进程id
	current_proc = &idle;	//idle 进程是第一个进程(boot进程)，也是唯一一个永远会存在的进程，它还有一个大家更熟悉的面孔，它就是 os 的 main 函数。
}

//分配一个进程号
int allocpid()
{
	static int PID = 1;
	return PID++;
}

//在进程表中查找未使用的进程。
//如果找到，则初始化在内核中运行所需的状态。
//如果没有空闲过程，或者内存分配失败，则返回 0
struct proc *allocproc(void)
{
	struct proc *p;
	for (p = pool; p < &pool[NPROC]; p++) {
		if (p->state == UNUSED) {	//循环遍历进程控制块结构体，找未使用的进程
			goto found;
		}
	}
	return 0;

found:
	p->pid = allocpid();	//分配一个进程号
	p->state = USED;			//将进程状态设置为已使用
	memset(&p->context, 0, sizeof(p->context));
	memset(p->trapframe, 0, PAGE_SIZE);
	memset((void *)p->kstack, 0, PAGE_SIZE);	//清空栈空间
	p->context.ra = (uint64)usertrapret;	//设置进程第一次运行入口地址是usertrapret。得进程能够从内核的S态返回U态并执行自己的代码。
	p->context.sp = p->kstack + PAGE_SIZE;	//因为切换进程是在内核态切换的，所以sp设置为进程内核栈顶
	return p;
}

// 调度程序远不会返回。它会循环执行以下操作：
// - 选择一个要运行的进程。
// - 调用 swtch 切换到该进程开始运行。
// - 最终，该进程通过 swtch 将控制权转回调度程序。
void scheduler(void)
{
	struct proc *p;
	for (;;) {
		for (p = pool; p < &pool[NPROC]; p++) {
			if (p->state == RUNNABLE) {
				/*
				* LAB1: you may need to init proc start time here
				*/
				p->state = RUNNING;
				current_proc = p;
				swtch(&idle.context, &p->context);
			}
		}
	}
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
// 切换到调度器。 必须仅持有 p->lock
// 并已经更改了 proc->state。保存并恢复
// intena，因为 intena 是此内核线程的属性，而不是此 CPU 的属性。它应该是 proc->intena 
// 和 proc->noff，但这会在少数持有锁但没有进程的位置出现问题。
void sched(void)
{
	struct proc *p = curr_proc();
	if (p->state == RUNNING)
		panic("sched running");
	swtch(&p->context, &idle.context);
}

// 放弃 CPU 进行下一轮调度。
void yield(void)
{
	current_proc->state = RUNNABLE;		//将当前进程状态设置为可执行
	sched();				//调用sched切换回进程调度器
}

//退出当前进程。
void exit(int code)
{
	struct proc *p = curr_proc();		//获取当前进程控制块
	infof("proc %d exit with %d", p->pid, code);
	p->state = UNUSED;	//修改进程状态位UNUSED
	finished();					//
	sched();						//切换回进程调度器
}
