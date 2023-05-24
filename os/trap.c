#include "trap.h"
#include "defs.h"
#include "loader.h"
#include "syscall.h"
#include "timer.h"

extern char trampoline[], uservec[];
extern void *userret(uint64);

//内核发生中断异常直接panic
void kerneltrap()
{
	if ((r_sstatus() & SSTATUS_SPP) == 0)
		panic("kerneltrap: not from supervisor mode");
	panic("trap from kernel\n");
}

// set up to take exceptions and traps while in the kernel.
void set_usertrap(void)
{
	w_stvec((uint64)uservec & ~0x3); // DIRECT
}

void set_kerneltrap(void)
{
	w_stvec((uint64)kerneltrap & ~0x3); // DIRECT
}

// set up to take exceptions and traps while in the kernel.
//在内核中设置为接受异常和陷阱。
void trap_init(void)
{
	set_kerneltrap();
}

//未知异常/中断
void unknown_trap()
{
	errorf("unknown trap: %p, stval = %p\n", r_scause(), r_stval());
	exit(-1);
}

// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//处理来自用户空间的中断、异常或系统调用。
//从 trampoline.S 调用
void usertrap()
{
	set_kerneltrap();
	struct trapframe *trapframe = curr_proc()->trapframe;

	if ((r_sstatus() & SSTATUS_SPP) != 0)
		panic("usertrap: not from user mode");

	uint64 cause = r_scause();
	if (cause & (1ULL << 63)) {	//判断是中断还是异常
		cause &= ~(1ULL << 63);
		switch (cause) {
		case SupervisorTimer:	//触发了一个S特权级的时钟中断
			tracef("time interrupt!\n");
			set_next_timer();		//重新设置一个10ms的计时器
			yield();						//yield 函数暂停当前应用并切换到下一个。
			break;
		default:
			unknown_trap();
			break;
		}
	} else {
		switch (cause) {
		case UserEnvCall:
			trapframe->epc += 4;
			syscall();
			break;
		case StoreMisaligned:
		case StorePageFault:
		case InstructionMisaligned:
		case InstructionPageFault:
		case LoadMisaligned:
		case LoadPageFault:
			printf("%d in application, bad addr = %p, bad instruction = %p, "
			       "core dumped.\n",
			       cause, r_stval(), trapframe->epc);
			exit(-2);
			break;
		case IllegalInstruction:
			printf("IllegalInstruction in application, core dumped.\n");
			exit(-3);
			break;
		default:
			unknown_trap();
			break;
		}
	}
	usertrapret();
}

// return to user space
//返回用户空间
void usertrapret()
{
	set_usertrap();		//设置为接受异常和陷阱
	struct trapframe *trapframe = curr_proc()->trapframe;
	trapframe->kernel_satp = r_satp(); //内核页表
	trapframe->kernel_sp =
		curr_proc()->kstack + PGSIZE; //进程的内核栈
	trapframe->kernel_trap = (uint64)usertrap;	//用户出现异常中断调用该函数
	trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

	w_sepc(trapframe->epc); //设置了sepc寄存器的值，回用户空间，调用完userret后就会执行epc所保存的那个地址的代码。
	// set up the registers that trampoline.S's sret will use
	// to get to user space.

	// set S Previous Privilege mode to User.	//将 S Previous 权限模式设置为用户
	uint64 x = r_sstatus();
	x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode	//将用户模式的 SPP 清零
	x |= SSTATUS_SPIE; // enable interrupts in user mode	//在用户模式下启用中断
	w_sstatus(x);

	// tell trampoline.S the user page table to switch to.
	// uint64 satp = MAKE_SATP(p->pagetable);
	//告诉 trampoline.S 要切换到的用户页表。
	//uint64 satp = MAKE_SATP(p->pagetable);
	userret((uint64)trapframe);	//定义在trampoline.S中 恢复 trapframe 结构体之中的保存的U态执行流数据。
}