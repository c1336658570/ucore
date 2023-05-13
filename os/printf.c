#include <stdarg.h>
#include "console.h"
#include "defs.h"
static char digits[] = "0123456789abcdef";

//打印xx，以base进制打印，sign是符号位
static void printint(int xx, int base, int sign)
{
	char buf[16];
	int i;
	uint x;

	if (sign && (sign = xx < 0))	//sign = 0时，不会执行后面那个语句，xx < 0时，sign=1，xx > 0时，sign=0
		x = -xx;
	else
		x = xx;

	i = 0;
	do {
		buf[i++] = digits[x % base];
	} while ((x /= base) != 0);

	if (sign)
		buf[i++] = '-';

	while (--i >= 0)
		consputc(buf[i]);
}

//打印地址，以16进制方式
static void printptr(uint64 x)
{
	int i;
	consputc('0');
	consputc('x');
	//循环输出x的16进制
	for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)	//一个字节以16进制需要循环两次，所以循环上限为(sizeof(uint64) * 2)
		consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);  //每次提取最高4位，然后将其转成16进制，然后输出
}

// Print to the console. only understands %d, %x, %p, %s.
void printf(char *fmt, ...)
{
	va_list ap;
	int i, c;
	char *s;

	if (fmt == 0)
		panic("null fmt");

	va_start(ap, fmt);
	for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
		if (c != '%') {
			consputc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if (c == 0)
			break;
		switch (c) {
		case 'd':
			printint(va_arg(ap, int), 10, 1);
			break;
		case 'x':
			printint(va_arg(ap, int), 16, 1);
			break;
		case 'p':
			printptr(va_arg(ap, uint64));
			break;
		case 's':
			if ((s = va_arg(ap, char *)) == 0)
				s = "(null)";
			for (; *s; s++)
				consputc(*s);
			break;
		case '%':
			consputc('%');
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%');
			consputc(c);
			break;
		}
	}
}