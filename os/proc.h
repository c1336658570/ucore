#ifndef PROC_H
#define PROC_H

#include "types.h"

#define NPROC (16)

//内核上下文切换的保存寄存器。
struct context {
	uint64 ra;
	uint64 sp;

	//被调用者保存的寄存器
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

//进程的状态，包括UNUSED（未初始化）、USED（基本初始化，未加载用户程序）、
//SLEEPING（睡眠中）、RUNNABLE（可运行）、RUNNING（运行中
//ZOMBIE（已经exit）。
enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

//进程控制块（PCB）
struct proc {
	enum procstate state; //进程状态
	int pid; //进程号
	uint64 ustack; //用户栈的虚拟地址(用户页表)
	uint64 kstack; ////内核栈的虚拟地址(内核页表)
	struct trapframe *trapframe; //trampoline.S   进程中断帧
	struct context context; //用于保存进程内核态的寄存器信息，进程切换时使用
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

#endif // PROC_H