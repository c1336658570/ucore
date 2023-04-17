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
	clean_bss();		// 清空 bss 段
	proc_init();		// 初始化线程池
	loader_init();	// 初始化 app_info_ptr 指针
	trap_init();		// 开启中断
	timer_init();		// 开启时钟中断，现在还没有
	run_all_app();	// 加载所有用户程序
	infof("start scheduler!");
	scheduler();		// 开始调度
}
