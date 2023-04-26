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
	set_next_timer();// 设置第一个 10ms 的计时器。
}

// mtimecmp 的作用是：一旦计数器 mtime 的值超过了 mtimecmp，就会触发一次时钟中断。
///设置下一个定时器中断，对set_timer()进行了封装
void set_next_timer()
{
	const uint64 timebase = CPU_FREQ / TICKS_PER_SEC;	//计算出 10ms(也就是一个tick) 之内计数器的增量
	set_timer(get_cycle() + timebase);	//读mtime的值，将其值和0ms(也就是一个tick) 之内计数器的增量相加写入mtimecmp
}