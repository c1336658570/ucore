#include "console.h"
#include "sbi.h"

/*
调用consputc 函数输出一个 char 到 shell，
consputc 函数其实就是调用了 sbi.c 之中的 console_putchar ，
console_putchar 函数的本质是调用了 sbi_call，
最终实现是使用 sbi 帮助我们包装好的 ecall 汇编代码
*/
void consputc(int c)
{
	console_putchar(c);
}

void console_init()
{
	// DO NOTHING
}