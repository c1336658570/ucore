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

//内存布局
//内核期望有 RAM
//供内核和用户页面使用
//从物理地址 0x80000000 到 PHYSTOP。
#define KERNBASE 0x80200000L
#define PHYSTOP (0x80000000 + 128 * 1024 * 1024) // we have 128M memroy

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
// 超过最大可能的虚拟地址。
// MAXVA实际上比Sv39允许的最大值小一个比特，以避免需要扩展带有高位设置的虚拟地址的符号。
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))	//VA的高位填充是根据39位来进行的。

// map the trampoline page to the highest address,
// in both user and kernel space.
//将蹦床页面映射到最高地址，
//在用户空间和内核空间。
#define USER_TOP (MAXVA)
#define TRAMPOLINE (USER_TOP - PGSIZE)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

// memory layout end
//内存布局结束

#define MAX_STR_LEN (200)

#endif // CONST_H