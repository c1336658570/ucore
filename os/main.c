#include "console.h"
#include "defs.h"
#include "loader.h"
#include "timer.h"
#include "trap.h"

void clean_bss()
{
	extern char s_bss[];
	extern char e_bss[];
	memset(s_bss, 0, e_bss - s_bss);
}

void main()
{
	clean_bss();		//清空bss
	printf("hello world!\n");
	proc_init();		//初始化所有进程控制块，为所有进程分配kstack，trapframe，等等...
	kinit();				//初始化所有内存，即将空闲内存按照页面挂到链表上
	kvm_init();			//为内核建立页表，设置satp寄存器，启动虚拟内存，内核是恒等映射，也是在这个函数设置的
	loader_init();	//初始化一些和加载app相关的信息
	trap_init();		//设置内核发生中断或异常后的处理程序入口
	timer_init();		//设置定时器
	//将所有app加载，并为每一个app分配一个进程，然后分配用户栈，进行虚拟内存到物理内存的映射，
	//此函数会调用loder.c中的bin_loader，bin_loader会设置tramframe->epc为BASEADDRESS，
	//在usertrapret中会将tramframe->epc设置到sepc中，然后调用trampoline.S中的userret
	//在userret中的最后一条指令sret会将当强的sepc设置为pc，然后就可以跳转到用户程序执行了
	run_all_app();	
	infof("start scheduler!");
	//通过调用scheduler，然后该函数调用switch，switch通过将proc.context.ra和sp加载到寄存器ra和sp
	//context.ra和sp是在proc.c中的allocproc中设置的，ra = usertrapret，sp = p->kstack + KSTACK_SIZE
	//最后一条ret指令，将ra和sp设置为当前pc和sp，就可以跳转到ra所指向的地址，即usertrapret开始执行
	//同时也将将栈切换到用户程序的内核栈
	//然后通过usertrapret，会将tramframe->epc设置到sepc中，然后调用trampoline.S中的userret
	//在userret中的最后一条指令sret会将但前的sepc设置为pc，然后就可以跳转到用户程序执行了
	//在userret中也切换了用户程序栈，从用户程序内核栈切换到了用户栈，在userret中通过将tramframe->sp加载到栈寄存器sp
	//tramframe->sp是在loader.c中run_all_app中调用bin_loader中设置的，在bin_loader中将其设置为p->ustack + USTACK_SIZE
	scheduler();
}