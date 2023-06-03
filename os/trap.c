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
	//将内核中断的函数入口设置为kernelvec.S中的kernelvec函数
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

/*
为什么始终不允许内核发生进程切换呢？只是由于我们的内核并没有并发的支持，
相关的数据结构没有锁或者其他机制保护。考虑这样一种情况，一个进程读写一个文件，
内核处理等待磁盘相应时，发生时钟中断切换到了其他进程，然而另一个进程也要读写同一个文件，
这就可能发生数据访问上的冲突，甚至导致磁盘出现错误的行为。这也是为什么内核态一直不处理时钟中断，
我们必须保证每一次内核的操作都是原子的，不能被打断。大家可以想一想，如果内核可以随时切换，
当前有那些数据结构可能被破坏。提示：想想 kalloc 分配到一半，进程 switch 切换到一半之类的。
*/
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
		irq = plic_claim();		//对于外部中断（Supervisor External），先使用 plic_claim() 获取中断请求号
		if (irq == UART0_IRQ) {	//UART串口的中断不需要处理，这个rustsbi替我们处理好了
			// do nothing
		} else if (irq == VIRTIO0_IRQ) {//如果是 VIRTIO0 的中断请求，就调用 virtio_disk_intr()
																		//函数来处理这个请求，并将处理结果通知到 virtio-disk.c 中。
																		//这个过程会将 buf->disk 置零，解除死循环的状态，程序可以继续进行。
		//virtio_disk_intr() 会把 buf->disk 置零，这样中断返回后死循环条件解除，程序可以继续运行。
		//具体代码在 virtio-disk.c 中。
			virtio_disk_intr();	
		} else if (irq) {		//如果是其他未知中断，则打印错误信息。
			infof("unexpected interrupt irq=%d\n", irq);
		}
		if (irq)	//在处理中断请求完成后，使用 plic_complete() 来告诉中断控制器已经成功处理了该请求。
			plic_complete(irq);								//表明中断已经处理完毕
		break;
	default:
		unknown_trap();
		break;
	}
}
/*
注解：
cause：参数，表示中断的类型。
SupervisorTimer 和 SupervisorExternal：常量，表示定时器和外部中断的类型。
yield()：调用该函数，如果进程发生了切换，则允许从当前进程切换到另一进程。
plic_claim()：调用该函数获取一个未处理的中断服务请求，并返回对应的中断号。
UART0_IRQ 和 VIRTIO0_IRQ：常量，表示 UART0 和 VIRTIO0 的中断号。
virtio_disk_intr()：调用该函数来处理 VIRTIO 块设备的中断请求，并在 virtio-disk.c 中通知处理结果。
plic_complete()：调用该函数，告诉中断控制器已经成功处理了指定的中断服务请求。
*/


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
