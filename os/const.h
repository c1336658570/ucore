#ifndef CONST_H
#define CONST_H

#define PAGE_SIZE (0x1000)

enum {
	STDIN = 0,
	STDOUT = 1,
	STDERR = 2,
};

// memory layout

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.

//0x80000000-0x8020000被rustsbi使用了
//内核希望有一定数量的 RAM 用于内核和用户页，从物理地址0x80000000到PHYSTOP。
#define KERNBASE 0x80200000L
#define PHYSTOP (0x80000000 + 128 * 1024 * 1024) // we have 128M memroy  有128M

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
// 超过最大可能的虚拟地址。
// MAXVA实际上比Sv39允许的最大值小一个比特，以避免需要扩展带有高位设置的虚拟地址的符号。
/*
下面的MAXVA其实并不是SV39中最大的虚拟地址(39位全为1)。我们设置它为(1<<38)是因为VA的高位填充是根据38位来进行的。
为了方便全部填充0，就不考虑大于(1<<38)的虚拟地址。 在我们的框架之中，va不可能大于MAXVA。
*/
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))	//VA的高位填充是根据38位来进行的。

// map the trampoline page to the highest address,
// in both user and kernel space.
//将蹦床页面映射到最高地址，
//在用户空间和内核空间。
#define USER_TOP (MAXVA)
#define TRAMPOLINE (USER_TOP - PGSIZE)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
/*
gpt给的trampoline和trapframe的解释
Trampoline和Trapframe是计算机系统中与异常处理和系统调用有关的概念。

Trampoline是一种用于处理系统调用的中间代码，它允许用户空间和内核空间之间切换执行。
当一个进程发出系统调用时，它的执行会进入内核空间，内核会在进程的内核栈上分配一个Trapframe结构，
保存当前进程断点的上下文和执行现场，然后跳转到内核中的Trampoline代码，继续处理系统调用。

Trapframe是一种数据结构，用于在内核和用户空间之间保存进程的执行现场。
当进程从用户空间切换到内核空间处理系统调用或异常时，进程的执行现场信息会被保存到Trapframe中，
当进程再次返回到用户空间时，Trapframe中的执行现场信息会被恢复，进程可以继续执行之前中断的指令。
*/

// memory layout end
//内存布局结束

#define MAX_STR_LEN (200)

#endif // CONST_H