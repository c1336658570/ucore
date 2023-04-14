#include "trap.h"
#include "defs.h"
#include "loader.h"
#include "syscall.h"

extern char trampoline[], uservec[], boot_stack_top[];
extern void *userret(uint64);

// set up to take exceptions and traps while in the kernel.
void trap_init(void)
{
	w_stvec((uint64)uservec & ~0x3);	//userver是在trampoline.S中定义的函数，写 stvec, 最后两位表明跳转模式，该实验始终为 0
}

//
// handle an interrupt, exception, or system call from user space. 处理来自用户空间的中断、异常或系统调用。
// called from trampoline.S	从 trampoline.S 调用
//
void usertrap(struct trapframe *trapframe)
{
	if ((r_sstatus() & SSTATUS_SPP) != 0)
		panic("usertrap: not from user mode");

	uint64 cause = r_scause();
	if (cause == UserEnvCall) {
		trapframe->epc += 4;
		syscall();
		return usertrapret(trapframe, (uint64)boot_stack_top);
	}
	switch (cause) {
	case StoreMisaligned:
	case StorePageFault:
	case LoadMisaligned:
	case LoadPageFault:
	case InstructionMisaligned:
	case InstructionPageFault:
		errorf("%d in application, bad addr = %p, bad instruction = %p, core "
		       "dumped.",
		       cause, r_stval(), trapframe->epc);
		break;
	case IllegalInstruction:
		errorf("IllegalInstruction in application, epc = %p, core dumped.",
		       trapframe->epc);
		break;
	default:
		errorf("unknown trap: %p, stval = %p sepc = %p", r_scause(),
		       r_stval(), r_sepc());
		break;
	}
	infof("switch to next app");
	run_next_app();
	printf("ALL DONE\n");
	shutdown();
}

//
// return to user space
//
void usertrapret(struct trapframe *trapframe, uint64 kstack)	//从S态返回U态
{
	//这里设置了返回地址sepc，并调用另外一个 userret 汇编函数来恢复 trapframe 结构体之中的保存的U态执行流数据。
	trapframe->kernel_satp = r_satp(); // kernel page table
	trapframe->kernel_sp = kstack + PGSIZE; // process's kernel stack
	trapframe->kernel_trap = (uint64)usertrap;
	trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

	w_sepc(trapframe->epc);	// 设置了sepc寄存器的值。
	// set up the registers that trampoline.S's sret will use
	// to get to user space.

	// set S Previous Privilege mode to User.
	uint64 x = r_sstatus();
	x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
	x |= SSTATUS_SPIE; // enable interrupts in user mode
	w_sstatus(x);

	// tell trampoline.S the user page table to switch to.
	// uint64 satp = MAKE_SATP(p->pagetable);
	userret((uint64)trapframe);	//定义在trampoline.S中 恢复 trapframe 结构体之中的保存的U态执行流数据。
}