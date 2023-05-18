#include "console.h"
#include "defs.h"
#include "loader.h"
#include "trap.h"

int threadid()
{
	return 0;
}

void clean_bss()
{
	extern char s_bss[];
	extern char e_bss[];
	memset(s_bss, 0, e_bss - s_bss);
}

void main()
{
	//bss节的内容初始化
	clean_bss();
	//打印hello world
	printf("hello wrold!\n");
	//中断异常后，进入的中断/异常函数的地址初始化
	trap_init();
	//loader初始化
	loader_init();
	/*调用run_neax_app运行第一个程序，在这个函数里会调用usertrapret，在usertrapret会设置
	trapframe->kernel_satp = r_satp(); // kernel page table	//内核页表
	trapframe->kernel_sp = kstack + PGSIZE; // process's kernel stack	//内核栈
	trapframe->kernel_trap = (uint64)usertrap;
	trapframe->kernel_hartid = r_tp();

	usertrapret会调用userret，在userret里面会设置sscratch寄存器，让其保存trapframe的地址
	所以以后调用userver的时候，sscratch保存的就是trapframe结构体的地址
	*/
	run_next_app();
}
