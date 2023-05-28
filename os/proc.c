#include "proc.h"
#include "defs.h"
#include "loader.h"
#include "trap.h"
#include "vm.h"
#include "queue.h"

struct proc pool[NPROC];
__attribute__((aligned(16))) char kstack[NPROC][PAGE_SIZE];
__attribute__((aligned(4096))) char trapframe[NPROC][TRAP_PAGE_SIZE];

extern char boot_stack_top[];
struct proc *current_proc;
struct proc idle;
struct queue task_queue;

int threadid()
{
	return curr_proc()->pid;
}

struct proc *curr_proc()
{
	return current_proc;
}

// initialize the proc table at boot time.
void proc_init()
{
	struct proc *p;
	for (p = pool; p < &pool[NPROC]; p++) {
		p->state = UNUSED;
		p->kstack = (uint64)kstack[p - pool];
		p->trapframe = (struct trapframe *)trapframe[p - pool];
	}
	idle.kstack = (uint64)boot_stack_top;
	idle.pid = IDLE_PID;
	current_proc = &idle;
	init_queue(&task_queue);	//初始化；队列，如队头队尾指针，队列是否为空
}

int allocpid()
{
	static int PID = 1;
	return PID++;
}

struct proc *fetch_task()
{
	int index = pop_queue(&task_queue);	//取出偏移
	if (index < 0) {
		debugf("No task to fetch\n");
		return NULL;
	}
	debugf("fetch task %d(pid=%d) to task queue\n", index, pool[index].pid);
	return pool + index;	//返回pcb的地址
}

void add_task(struct proc *p)
{
	push_queue(&task_queue, p - pool);	//p-pool可以获得当前pcb相对于第1个pcb之间相差了多少个pcb
	debugf("add task %d(pid=%d) to task queue\n", p - pool, p->pid);
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel.
// If there are no free procs, or a memory allocation fails, return 0.
struct proc *allocproc()
{
	struct proc *p;
	for (p = pool; p < &pool[NPROC]; p++) {
		if (p->state == UNUSED) {
			goto found;
		}
	}
	return 0;

found:
	// init proc
	p->pid = allocpid();
	p->state = USED;
	p->ustack = 0;
	p->max_page = 0;
	p->parent = NULL;
	p->exit_code = 0;
	p->pagetable = uvmcreate((uint64)p->trapframe);	//创建一个页表，在创建页表的同时会将trampoline和trapframe映射
	p->program_brk = 0;
  p->heap_bottom = 0;
	memset(&p->context, 0, sizeof(p->context));
	memset((void *)p->kstack, 0, KSTACK_SIZE);
	memset((void *)p->trapframe, 0, TRAP_PAGE_SIZE);
	p->context.ra = (uint64)usertrapret;
	p->context.sp = p->kstack + KSTACK_SIZE;
	return p;
}

// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler()
{
	struct proc *p;
	for (;;) {
		/*int has_proc = 0;
		for (p = pool; p < &pool[NPROC]; p++) {
			if (p->state == RUNNABLE) {
				has_proc = 1;
				tracef("swtich to proc %d", p - pool);
				p->state = RUNNING;
				current_proc = p;
				swtch(&idle.context, &p->context);
			}
		}
		if(has_proc == 0) {
			panic("all app are over!\n");
		}*/
		p = fetch_task();	//通过调用fetch_task可以从队列中取出队头pcb的地址，然后将这个地址给p
		if (p == NULL) {
			panic("all app are over!\n");
		}
		tracef("swtich to proc %d", p - pool);
		p->state = RUNNING;
		current_proc = p;
		swtch(&idle.context, &p->context);
	}
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched()
{
	struct proc *p = curr_proc();
	if (p->state == RUNNING)
		panic("sched running");
	swtch(&p->context, &idle.context);
}

// Give up the CPU for one scheduling round.
void yield()
{
	current_proc->state = RUNNABLE;
	add_task(current_proc);
	sched();
}

// Free a process's page table, and free the
// physical memory it refers to.
//释放一个进程的页表，并释放它所引用的物理内存。
void freepagetable(pagetable_t pagetable, uint64 max_page)
{
	uvmunmap(pagetable, TRAMPOLINE, 1, 0);	//删除从TRAMPOLINE开始1页的虚拟地址到物理地址的映射，但是不释放内存
	uvmunmap(pagetable, TRAPFRAME, 1, 0);		//删除从TRAPFRAME开始1页的虚拟地址到物理地址的映射，但是不释放内存
	uvmfree(pagetable, max_page);	//删除从虚拟地址0开始，共max_page个页面的虚拟地址到物理地址的映射，并释放页帧的物理内存，也也释放了页表的内存
}

//清除进程所占用的资源，让进程回到执行完函数proc_init的状态，会清除TRAMPOLINE和TRAPFRAME的映射
void freeproc(struct proc *p)
{
	if (p->pagetable)
		freepagetable(p->pagetable, p->max_page);
	p->pagetable = 0;
	p->state = UNUSED;
}

int fork()
{
	struct proc *np;
	struct proc *p = curr_proc();
	//分配一个新的进程PCB，注意页表的初始化也在allocproc中完成了
	if ((np = allocproc()) == 0) {
		panic("allocproc\n");
	}
	// Copy user memory from parent to child.
	//将用户内存从父母复制到孩子。
	//把父进程页表对应的页先拷贝一份，然后建立一个对这些新页有同样映射的页表。如果失败会返回-1
	if (uvmcopy(p->pagetable, np->pagetable, p->max_page) < 0) {
		panic("uvmcopy\n");
	}
	np->max_page = p->max_page;
	// copy saved user registers.
	//复制保存的用户寄存器。
	*(np->trapframe) = *(p->trapframe);
	// Cause fork to return 0 in the child.
	//导致 fork 在子进程中返回 0。
	np->trapframe->a0 = 0;	//设置子进程返回值为0
	np->parent = p;
	np->state = RUNNABLE;
	add_task(np);	//将np添加到就绪队列
	return np->pid;	//返回子进程的pid
}

//由于trapframe和trampoline是可以复用的（每个进程都一样），所以我们并不会把他们unmap。
//而对于用户真正的数据，就会删掉映射的同时把物理页面也free掉。
//传入待执行测例的文件名。之后会找到文件名对应的id。如果存在对应文件，就会执行内存的释放。
//exec要干的事情和bin_loader是很相似的。事实上，
//不同点在于，exec 需要先清理并回收掉当前进程占用的资源，目前只有内存。
int exec(char *name)
{
	int id = get_id_by_name(name);	//通过name得到是第几个可执行程序
	if (id < 0)
		return -1;
	struct proc *p = curr_proc();
	uvmunmap(p->pagetable, 0, p->max_page, 1);	//删除从p->pagetable开始的p->max_page页映射。并释放物理内存。不会清除trapframe和trampoline的映射，因为它们在虚拟地址的最高端
	p->max_page = 0;
	loader(id, p);	//加载id所对应的程序到该进程
	return 0;
}

//pid 表示要等待结束的子进程的进程 ID，如果为0或者-1的话表示等待任意一个子进程结束；
//status 表示保存子进程返回值的地址，如果这个地址为0的话表示不必保存。
//返回值：如果出现了错误则返回 -1；否则返回结束的子进程的进程ID。
//如果子进程存在且尚未完成，该系统调用阻塞等待。
//pid非法或者指定的不是该进程的子进程或传入的地址status不为0但是不合法均会导致错误。
//wait 的思路就是遍历进程数组，看有没有和pid匹配的进程。如果有且已经结束(ZOMBIE态），
//按要求返回。如果指定进程不存在或者不是当前进程子进程，返回错误。
//如果子进程存在但未结束，调用 sched 切换到其他进程来等待子进程结束。
int wait(int pid, int *code)
{
	struct proc *np;
	int havekids;
	struct proc *p = curr_proc();	//获取当前正在执行的进程的PCB

	for (;;) {
		// Scan through table looking for exited children.
		havekids = 0;
		for (np = pool; np < &pool[NPROC]; np++) {	//遍历PCB，找子进程
			if (np->state != UNUSED && np->parent == p &&
			    (pid <= 0 || np->pid == pid)) {
				havekids = 1;
				if (np->state == ZOMBIE) {
					// Found one.
					np->state = UNUSED;	//修改子进程状态为UNUSED
					pid = np->pid;			//修改返回值
					*code = np->exit_code;	//传出参数，将子进程结束时的返回值传出
					return pid;
				}
			}
		}
		if (!havekids) {	//没有子进程
			return -1;
		}
		//遍历一遍未找到ZOMBIE状态的子进程，就将当前进程添加到就绪队列，然后等待被调度
		p->state = RUNNABLE;	
		add_task(p);
		sched();
	}
}

// Exit the current process.
void exit(int code)
{
	struct proc *p = curr_proc();
	p->exit_code = code;
	debugf("proc %d exit with %d\n", p->pid, code);
	freeproc(p);	//释放进程的占有所有资源，让进程回到刚执行完proc_init的状态（UNUSED）
	if (p->parent != NULL) {
		// Parent should `wait`
		p->state = ZOMBIE;
	}
	// Set the `parent` of all children to NULL
	//将所有孩子的 `parent` 设置为 NULL
	struct proc *np;
	for (np = pool; np < &pool[NPROC]; np++) {
		if (np->parent == p) {
			np->parent = NULL;
		}
	}
	sched();
}

// Grow or shrink user memory by n bytes.
// Return 0 on succness, -1 on failure.
//将用户内存增加或缩小 n 字节。
//成功返回 0，失败返回 -1。
int growproc(int n)
{
	uint64 program_brk;
	struct proc *p = curr_proc();
	program_brk = p->program_brk;
	int new_brk = program_brk + n - p->heap_bottom;
	if(new_brk < 0){
		return -1;
	}
	if(n > 0){
		if((program_brk = uvmalloc(p->pagetable, program_brk, program_brk + n, PTE_W)) == 0) {
			return -1;
		}
	} else if(n < 0){
		program_brk = uvmdealloc(p->pagetable, program_brk, program_brk + n);
	}
	p->program_brk = program_brk;
	return 0;
}
