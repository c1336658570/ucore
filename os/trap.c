#include "trap.h"
#include "defs.h"
#include "loader.h"
#include "plic.h"
#include "syscall.h"
#include "timer.h"
#include "virtio.h"

extern char trampoline[], uservec[];
extern char userret[], kernelvec[];

void kerneltrap();

// set up to take exceptions and traps while in the kernel.
void set_usertrap()
{
	w_stvec(((uint64)TRAMPOLINE + (uservec - trampoline)) & ~0x3); // DIRECT
}

void set_kerneltrap()
{	
	//将内核中断的函数入口设置为kernelvec
	w_stvec((uint64)kernelvec & ~0x3); // DIRECT
}

// set up to take exceptions and traps while in the kernel.
void trap_init()
{
	// intr_on();
	set_kerneltrap();
	w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
}

void unknown_trap()
{
	errorf("unknown trap: %p, stval = %p", r_scause(), r_stval());
	exit(-1);
}

// 外部中断处理函数
void devintr(uint64 cause)
{
	int irq;
	switch (cause) {
	case SupervisorTimer:	//时钟中断
		set_next_timer();
		// if form user, allow yield
		//如果是用户，允许 yield
		// 时钟中断如果发生在内核态，不切换进程，原因分析在下面
		// 如果发生在用户态，照常处理
		if ((r_sstatus() & SSTATUS_SPP) == 0) {
			yield();
		}
		break;
	case SupervisorExternal:	//外部中断
		irq = plic_claim();
		if (irq == UART0_IRQ) {	//UART串口的终端不需要处理，这个rustsbi替我们处理好了
			// do nothing
		} else if (irq == VIRTIO0_IRQ) {			//我们等的就是这个中断
		//virtio_disk_intr() 会把 buf->disk 置零，这样中断返回后死循环条件解除，程序可以继续运行。
		//具体代码在 virtio-disk.c 中。
			virtio_disk_intr();	
		} else if (irq) {										
			infof("unexpected interrupt irq=%d\n", irq);
		}
		if (irq)
			plic_complete(irq);								//表明中断已经处理完毕
		break;
	default:
		unknown_trap();
		break;
	}
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap()
{
	set_kerneltrap();
	struct trapframe *trapframe = curr_proc()->trapframe;
	tracef("trap from user epc = %p", trapframe->epc);
	if ((r_sstatus() & SSTATUS_SPP) != 0)
		panic("usertrap: not from user mode");

	uint64 cause = r_scause();
	if (cause & (1ULL << 63)) {
		devintr(cause & 0xff);
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
			errorf("%d in application, bad addr = %p, bad instruction = %p, "
			       "core dumped.",
			       cause, r_stval(), trapframe->epc);
			exit(-2);
			break;
		case IllegalInstruction:
			errorf("IllegalInstruction in application, core dumped.");
			exit(-3);
			break;
		default:
			unknown_trap();
			break;
		}
	}
	usertrapret();
}

//
// return to user space
//
void usertrapret()
{
	set_usertrap();
	struct trapframe *trapframe = curr_proc()->trapframe;
	trapframe->kernel_satp = r_satp(); // kernel page table
	trapframe->kernel_sp =
		curr_proc()->kstack + KSTACK_SIZE; // process's kernel stack
	trapframe->kernel_trap = (uint64)usertrap;
	trapframe->kernel_hartid = r_tp(); // unuesd

	w_sepc(trapframe->epc);
	// set up the registers that trampoline.S's sret will use
	// to get to user space.

	// set S Previous Privilege mode to User.
	uint64 x = r_sstatus();
	x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
	x |= SSTATUS_SPIE; // enable interrupts in user mode
	w_sstatus(x);

	// tell trampoline.S the user page table to switch to.
	uint64 satp = MAKE_SATP(curr_proc()->pagetable);
	uint64 fn = TRAMPOLINE + (userret - trampoline);
	tracef("return to user @ %p", trapframe->epc);
	((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}

void kerneltrap()
{
	// 老三样，不过在这里把处理放到了 C 代码中
	uint64 sepc = r_sepc();
	uint64 sstatus = r_sstatus();
	uint64 scause = r_scause();

	debugf("kernel tarp: epc = %p, cause = %d", sepc, scause);

	if ((sstatus & SSTATUS_SPP) == 0)
		panic("kerneltrap: not from supervisor mode");

	if (scause & (1ULL << 63)) {	//是中断就进入这个分支，通过scause最高位判断是中断还是异常
		// 可能发生时钟中断和外部中断，我们的主要目标是处理外部中断
		devintr(scause & 0xff);
	} else {		//异常进入这个分支
		// kernel 发生异常就挣扎了，肯定出问题了，杀掉用户线程跑路
		errorf("invalid trap from kernel: %p, stval = %p sepc = %p\n",
		       scause, r_stval(), sepc);
		exit(-1);
	}
	// the yield() may have caused some traps to occur,
	// so restore trap registers for use by kernelvec.S's sepc instruction.
	//yield() 可能导致了一些陷阱的发生，
	//因此恢复陷阱寄存器以供 kernelvec.S 的 sepc 指令使用。	
	w_sepc(sepc);
	w_sstatus(sstatus);
}
