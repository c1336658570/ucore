#include "timer.h"
#include "riscv.h"
#include "sbi.h"

/// read the `mtime` regiser，统计处理器自上电以来经过了多少个内置时钟的时钟周期。
uint64 get_cycle()
{
	return r_time();
}

/// Enable timer interrupt
void timer_init()
{
	// Enable supervisor timer interrupt
	w_sie(r_sie() | SIE_STIE);	// 设置 ``sie.stie`` 使得 S 特权级时钟中断不会被屏蔽；
	//STIE的意思是Supervisor timer interrupt S态的时中中断
	set_next_timer();// 设置第一个 10ms 的计时器。
}

// mtimecmp 的作用是：一旦计数器 mtime 的值超过了 mtimecmp，就会触发一次时钟中断。
//这使得我们可以方便的通过设置 mtimecmp 的值来决定下一次时钟中断何时触发。	
//mtime和mtimecmp都是M态寄存器，运行在 M 特权级的 SEE 已经预留了相应的接口，我们可以调用它们来间接实现计时器的控制
///设置下一个定时器中断，对set_timer()进行了封装
void set_next_timer()
{
	const uint64 timebase = CPU_FREQ / TICKS_PER_SEC;	//计算出 10ms(也就是一个tick) 之内计数器的增量
	//get_cycle()函数可以取得当前 mtime 计数器的值；
	//set_timer()调用，是一个由SEE提供的标准SBI接口函数，它可以用来设置mtimecmp的值。
	set_timer(get_cycle() + timebase);	//读mtime的值，将其值和10ms(也就是一个tick) 之内计数器的增量相加写入mtimecmp
}