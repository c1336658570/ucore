#ifndef RISCV_H
#define RISCV_H

#include "types.h"

/*
riscv内联汇编格式
asm [volatile] (
	"汇编指令"
	:输出操作数列表（可选）
	:输入操作数列表（可选）
	:可能影响的寄存器或者存储器（可选）
)
例：
int foo(int a, int b) {
	int c;
	asm volatile(
		"add %0, %1, %2"	//
		: "=r"(c)			//输出操作数列表
		: "r"(a), "r"(b)	//输入操作数列表
	);
	return c;
}
*/

// which hart (core) is this?
static inline uint64 r_mhartid()	//寄存器mhartid包含硬件线程ID整数值。硬件线程 ID 在一个多处理器系统中并不要求一定是连续的，但是至少应该有一个硬件线程，它的 ID 是 0。
{
	uint64 x;
	asm volatile("csrr %0, mhartid" : "=r"(x)); //读取mhartid寄存器并放入到x中
	return x;	//将x返回
}

// Machine Status Register, mstatus
//较低权限的 sstatus 和 ustatus 寄存器几乎同理	xPP 之前的特权级别	xPIE 之前的中断使能		xIE 中断使能
#define MSTATUS_MPP_MASK (3L << 11) // previous mode.保存上一次运行时的特权级别
#define MSTATUS_MPP_M (3L << 11)	//11、12位，设置mpp位
#define MSTATUS_MPP_S (1L << 11)	
#define MSTATUS_MPP_U (0L << 11)	
#define MSTATUS_MIE (1L << 3) // machine-mode interrupt enable.MIE位，开中断

static inline uint64 r_mstatus()	//机器状态寄存器mstatus。mstatus 在 H 和 S 特权级 ISA 受限的视图，分别出现在 hstatus 和 sstatus 寄存器中。
{
	uint64 x;
	asm volatile("csrr %0, mstatus" : "=r"(x));	//读mstatus到uint64 x
	return x;
}

static inline void w_mstatus(uint64 x)
{
	asm volatile("csrw mstatus, %0" : : "r"(x));//写x到mstatus，其中输出参数列表被忽略，只有输入参数列表
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_mepc(uint64 x)
{
	asm volatile("csrw mepc, %0" : : "r"(x));	//写x到mepc
}

// Supervisor Status Register, sstatus
//xPP 之前的特权级别	xPIE 之前的中断使能		xIE 中断使能
#define SSTATUS_SPP (1L << 8) // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1) // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0) // User Interrupt Enable

static inline uint64 r_sstatus() 	//sstatus：记录一些比较重要的状态，比如是否允许中断异常嵌套。
{
	uint64 x;
	asm volatile("csrr %0, sstatus" : "=r"(x));	//读sstatus到x
	return x;
}

static inline void w_sstatus(uint64 x)
{
	asm volatile("csrw sstatus, %0" : : "r"(x));	//写x到sstatus
}

// Supervisor Interrupt Pending
static inline uint64 r_sip()
{
	uint64 x;
	asm volatile("csrr %0, sip" : "=r"(x));		//读sip到x
	return x;
}

static inline void w_sip(uint64 x)
{
	asm volatile("csrw sip, %0" : : "r"(x));	//写x到sip
}

//mie 寄存器包含了相应的中断使能位，sie 和 uie 功能相似。
// Supervisor Interrupt Enable	xTIE 时钟中断使能位	xSIE 软件中断使能位	xEIE 外部中断使能位
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software
static inline uint64 r_sie()
{
	uint64 x;
	asm volatile("csrr %0, sie" : "=r"(x));		//读sie到x
	return x;
}

static inline void w_sie(uint64 x)
{
	asm volatile("csrw sie, %0" : : "r"(x));	//写x到sie
}

// Machine-mode Interrupt Enable	xTIE 时钟中断使能位	xSIE 软件中断使能位	xEIE 外部中断使能位
#define MIE_MEIE (1L << 11) // external
#define MIE_MTIE (1L << 7) // timer
#define MIE_MSIE (1L << 3) // software
static inline uint64 r_mie()
{
	uint64 x;
	asm volatile("csrr %0, mie" : "=r"(x));		//读mie到x
	return x;
}

static inline void w_mie(uint64 x)
{
	asm volatile("csrw mie, %0" : : "r"(x));	//写x到mie
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_sepc(uint64 x) //sepc 处理完毕中断异常之后需要返回的PC值。
{
	asm volatile("csrw sepc, %0" : : "r"(x));	//写x到sepc
}

static inline uint64 r_sepc()
{
	uint64 x;
	asm volatile("csrr %0, sepc" : "=r"(x));	//读sepc到x
	return x;
}

// Machine Exception Delegation
static inline uint64 r_medeleg()
{
	uint64 x;
	asm volatile("csrr %0, medeleg" : "=r"(x));	//读medeleg到x
	return x;
}

static inline void w_medeleg(uint64 x)
{
	asm volatile("csrw medeleg, %0" : : "r"(x));//写x到medeleg
}

// Machine Interrupt Delegation
static inline uint64 r_mideleg()
{
	uint64 x;
	asm volatile("csrr %0, mideleg" : "=r"(x));	//读mideleg到x
	return x;
}

static inline void w_mideleg(uint64 x)
{
	asm volatile("csrw mideleg, %0" : : "r"(x));//写x到mideleg
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void w_stvec(uint64 x)	//stvec：处理异常的函数的起始地址。
{
	asm volatile("csrw stvec, %0" : : "r"(x));	//写
}

static inline uint64 r_stvec()
{
	uint64 x;
	asm volatile("csrr %0, stvec" : "=r"(x));	//读
	return x;
}

// Machine-mode interrupt vector
static inline void w_mtvec(uint64 x)
{
	asm volatile("csrw mtvec, %0" : : "r"(x));	//写
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
static inline void w_satp(uint64 x)
{
	asm volatile("csrw satp, %0" : : "r"(x));
}

static inline uint64 r_satp()
{
	uint64 x;
	asm volatile("csrr %0, satp" : "=r"(x));
	return x;
}

// Supervisor Scratch register, for early trap handler in trampoline.S.
static inline void w_sscratch(uint64 x)
{
	asm volatile("csrw sscratch, %0" : : "r"(x));
}

static inline void w_mscratch(uint64 x)
{
	asm volatile("csrw mscratch, %0" : : "r"(x));
}

// Supervisor Trap Cause
static inline uint64 r_scause() //scause: 它用于记录异常和中断的原因。它的最高位为1是中断，否则是异常。其低位决定具体的种类。
{
	uint64 x;
	asm volatile("csrr %0, scause" : "=r"(x));
	return x;
}

// Supervisor Trap Value
static inline uint64 r_stval() //stval: 产生异常的指令的地址
{
	uint64 x;
	asm volatile("csrr %0, stval" : "=r"(x));
	return x;
}

// Machine-mode Counter-Enable
static inline void w_mcounteren(uint64 x)
{
	asm volatile("csrw mcounteren, %0" : : "r"(x));
}

static inline uint64 r_mcounteren()
{
	uint64 x;
	asm volatile("csrr %0, mcounteren" : "=r"(x));
	return x;
}

// machine-mode cycle counter
static inline uint64 r_time()
{
	uint64 x;
	asm volatile("csrr %0, time" : "=r"(x));
	return x;
}

// enable device interrupts
static inline void intr_on()
{
	w_sstatus(r_sstatus() | SSTATUS_SIE); 
}

// disable device interrupts
static inline void intr_off()
{
	w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int intr_get()
{
	uint64 x = r_sstatus();
	return (x & SSTATUS_SIE) != 0;
}

static inline uint64 r_sp()
{
	uint64 x;
	asm volatile("mv %0, sp" : "=r"(x));
	return x;
}

// read and write tp, the thread pointer, which holds
// this core's hartid (core number), the index into cpus[].
static inline uint64 r_tp()
{
	uint64 x;
	asm volatile("mv %0, tp" : "=r"(x));
	return x;
}

static inline void w_tp(uint64 x)
{
	asm volatile("mv tp, %0" : : "r"(x));
}

static inline uint64 r_ra()
{
	uint64 x;
	asm volatile("mv %0, ra" : "=r"(x));
	return x;
}

// flush the TLB.
static inline void sfence_vma()
{
	// the zero, zero means flush all TLB entries.
	asm volatile("sfence.vma zero, zero");
}

#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12 // bits of offset within a page

#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))

#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // 1 -> user can access

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte)&0x3FF)

// extract the three 9-bit page table indices from a virtual address.
#define PXMASK 0x1FF // 9 bits
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va) ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

#endif // RISCV_H