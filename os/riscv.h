#ifndef RISCV_H
#define RISCV_H

#include "types.h"

// which hart (core) is this?
static inline uint64 r_mhartid()
{
	uint64 x;
	asm volatile("csrr %0, mhartid" : "=r"(x));
	return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3) // machine-mode interrupt enable.

static inline uint64 r_mstatus()
{
	uint64 x;
	asm volatile("csrr %0, mstatus" : "=r"(x));
	return x;
}

static inline void w_mstatus(uint64 x)
{
	asm volatile("csrw mstatus, %0" : : "r"(x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_mepc(uint64 x)
{
	asm volatile("csrw mepc, %0" : : "r"(x));
}

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8) // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1) // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0) // User Interrupt Enable

static inline uint64 r_sstatus()
{
	uint64 x;
	asm volatile("csrr %0, sstatus" : "=r"(x));
	return x;
}

static inline void w_sstatus(uint64 x)
{
	asm volatile("csrw sstatus, %0" : : "r"(x));
}

// Supervisor Interrupt Pending
static inline uint64 r_sip()
{
	uint64 x;
	asm volatile("csrr %0, sip" : "=r"(x));
	return x;
}

static inline void w_sip(uint64 x)
{
	asm volatile("csrw sip, %0" : : "r"(x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software
static inline uint64 r_sie()
{
	uint64 x;
	asm volatile("csrr %0, sie" : "=r"(x));
	return x;
}

static inline void w_sie(uint64 x)
{
	asm volatile("csrw sie, %0" : : "r"(x));
}

// Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11) // external
#define MIE_MTIE (1L << 7) // timer
#define MIE_MSIE (1L << 3) // software
static inline uint64 r_mie()
{
	uint64 x;
	asm volatile("csrr %0, mie" : "=r"(x));
	return x;
}

static inline void w_mie(uint64 x)
{
	asm volatile("csrw mie, %0" : : "r"(x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_sepc(uint64 x)
{
	asm volatile("csrw sepc, %0" : : "r"(x));
}

static inline uint64 r_sepc()
{
	uint64 x;
	asm volatile("csrr %0, sepc" : "=r"(x));
	return x;
}

// Machine Exception Delegation
static inline uint64 r_medeleg()
{
	uint64 x;
	asm volatile("csrr %0, medeleg" : "=r"(x));
	return x;
}

static inline void w_medeleg(uint64 x)
{
	asm volatile("csrw medeleg, %0" : : "r"(x));
}

// Machine Interrupt Delegation
static inline uint64 r_mideleg()
{
	uint64 x;
	asm volatile("csrr %0, mideleg" : "=r"(x));
	return x;
}

static inline void w_mideleg(uint64 x)
{
	asm volatile("csrw mideleg, %0" : : "r"(x));
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void w_stvec(uint64 x)
{
	asm volatile("csrw stvec, %0" : : "r"(x));
}

static inline uint64 r_stvec()
{
	uint64 x;
	asm volatile("csrr %0, stvec" : "=r"(x));
	return x;
}

// Machine-mode interrupt vector
static inline void w_mtvec(uint64 x)
{
	asm volatile("csrw mtvec, %0" : : "r"(x));
}

// use riscv's sv39 page table scheme.
//使用 riscv 的 sv39 页表方案。
//第63位变为1	SATP寄存器的60-63位为MODE，通过设置MODE可以打开分页
//当 MODE 设置为 0 的时候，代表所有访存都被视为物理地址
//而设置为 8 的时候，SV39分页机制被启用，所有S/U特权级的访存被视为一个39位的虚拟地址，
//它们需要先经过 MMU 的地址转换流程
#define SATP_SV39 (8L << 60)	//将SATP寄存器的MODE设置为8，MODE保存在60-63位，MODE设置为8代表启用分页，采用SV39
//SATP寄存器0-43位为物理页号（PPN），用于指定根页表的基地址（多级页表根节点所在的物理页号）。44-59位为ASID（WARL），60-63位为MODE，控制是否打开分页机制，设置为8打开SV39
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))	//通过这个可以将MODE设置为8即打开SV39分页，然后还会将给的pagetable右移12位获得页号，将MODE和页号组合返回stap


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
static inline uint64 r_scause()
{
	uint64 x;
	asm volatile("csrr %0, scause" : "=r"(x));
	return x;
}

// Supervisor Trap Value
static inline uint64 r_stval()
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

#define PGSIZE 4096 // bytes per page	每页字节数
#define PGSHIFT 12 // bits of offset within a page	页面内的偏移位数

//(sz + 4095) & ~4095  将sz和4095相加，并将低12位清空，以4K为粒度，获取页地址，将sz向上对齐到页面边界上。
#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))		//低12位清0
#define PGALIGNED(a) (((a) & (PGSIZE - 1)) == 0)	//判断低12位是否为0,即是否页面对齐

#define PTE_V (1L << 0) // valid有效的
#define PTE_R (1L << 1)	//读
#define PTE_W (1L << 2)	//写
#define PTE_X (1L << 3)	//执行
#define PTE_U (1L << 4) // 1 -> user can access 用户可以访问
a
// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)	//页表的物理地址转成PTE（页表项）
//右移12位相当于将低12位清零，也就是按照4K对齐，也等于清除页内偏移地址，让其变为一个页帧的首地址
//然后左移10位让其变为一个页表项（PTE），因为页表项低10位为一些权限，V，R，W，X，U，G，A，D，RSW，
//然后10-54位是PPN，10-18为PPN[0]，19-27为PPN[1]，28-53为PPN[2]，54-63为reserved


//PTE到物理地址		右移10位是因为页表项低10位为一些权限，V，R，W，X，U，G，A，D，RSW，
//然后10-54位是PPN，10-18为PPN[0]，19-27为PPN[1]，28-53为PPN[2]，54-63为reserved
#define PTE2PA(pte) (((pte) >> 10) << 12)	//将该页表项的高44位（也就是下一个页表的页号）取出

#define PTE_FLAGS(pte) ((pte)&0x3FF)

// extract the three 9-bit page table indices from a virtual address.
//从虚拟地址中提取三个 9 位页表索引。
#define PXMASK 0x1FF // 9 bits
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))  //PGSHIFT为12,在296行
#define PX(level, va) ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)	//将va右移，只保留最低9位，也就是一个PPN，通过PPN和下一级页表（下一级页表的起始地址）可以找到下一级页表项

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
// 超出最大可能的虚拟地址。
// MAXVA实际上比Sv39允许的最大值小一个比特，以避免需要符号扩展具有高位设置的虚拟地址。
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

typedef uint64 pte_t;
typedef uint64 pde_t;
typedef uint64 *pagetable_t; // 512 PTEs

#endif // RISCV_H