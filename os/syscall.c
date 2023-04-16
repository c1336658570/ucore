#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "trap.h"

uint64 sys_write(int fd, char *str, uint len)
{
	debugf("sys_write fd = %d str = %x, len = %d", fd, str, len);	//宏，打印一些信息
	if (fd != STDOUT)
		return -1;	//如果不是标准输出，就返回
	for (int i = 0; i < len; ++i) {
		console_putchar(str[i]);	//输出要打印的字符
	}
	return len;		
}

__attribute__((noreturn)) void sys_exit(int code)
{
	debugf("sysexit(%d)", code);	//宏打印一些信息
	run_next_app();
	printf("ALL DONE\n");
	shutdown();
	__builtin_unreachable();
}

/*__attribute__((noreturn))是一个函数属性，指示该函数不会返回到调用方，
也就是说，函数永远不会正常退出，而是以某种退出方式提前结束函数。

__builtin_unreachable()是GCC内置函数，它表示程序在此处永远不会执行到，
可以被用于编写不可到达的代码段，从而产生优化效果。
*/

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = (struct trapframe *)trap_page;
	int id = trapframe->a7, ret;//a7 用来传递 syscall ID
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
				 //约定寄存器 a0~a6 保存系统调用的参数， a0 保存系统调用的返回值
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], (char *)args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
