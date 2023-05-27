#ifndef PROC_H
#define PROC_H

#include "riscv.h"
#include "types.h"

#define NPROC (16)

// Saved registers for kernel context switches.
struct context {
	uint64 ra;
	uint64 sp;

	// callee-saved
	uint64 s0;
	uint64 s1;
	uint64 s2;
	uint64 s3;
	uint64 s4;
	uint64 s5;
	uint64 s6;
	uint64 s7;
	uint64 s8;
	uint64 s9;
	uint64 s10;
	uint64 s11;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
	enum procstate state; // Process state	进程的状态
	int pid; // Process ID		进程的唯一标识符
	pagetable_t pagetable; // User page table		进程的页表，用于将虚拟地址映射到物理地址。
	uint64 ustack;		//用户态栈的顶部地址，即栈向下生长的最高地址。
	uint64 kstack; // Virtual address of kernel stack	内核态栈的顶部地址。用于保存进程从用户态切换到内核态时的上下文信息。
	struct trapframe *trapframe; // data page for trampoline.S	指向进程的中断帧，当进程从用户态切换到内核态时，操作系统会保存进程的上下文信息到中断帧中。
	struct context context; // swtch() here to run process	用于保存进程上下文信息，包括CPU寄存器和内存映射等。
	uint64 max_page;		//进程拥有的最大物理页面数。
	uint64 program_brk;		//进程的堆顶地址，即堆向上生长的最高地址。
	uint64 heap_bottom;		//进程的堆底地址，即堆向下生长的最低地址。
	/*
	* LAB1: you may need to add some new fields here
	*/
};

/*
* LAB1: you may need to define struct for TaskInfo here
*/

struct proc *curr_proc();
void exit(int);
void proc_init();
void scheduler() __attribute__((noreturn));
void sched();
void yield();
struct proc *allocproc();
// swtch.S
void swtch(struct context *, struct context *);

int growproc(int n);

#endif // PROC_H
