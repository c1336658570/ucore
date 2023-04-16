#include"loader.h"
#include"defs.h"
#include"trap.h"

static int app_cur,app_num;
static uint64* app_info_ptr;
extern char _app_num[];//link_app.S中定义，一个地址，是可执行文件的个数地址，此地址+8就是将二进制文件包含到汇编程序中的那条指令的地址
extern char userret[];//trampoline.S
extern char boot_stack_top[];//entry.S，内核栈的栈顶地址
extern char ekernel[];//kernel_app.S

void loader_init()
{
    if((uint64)ekernel >= BASE_ADDRESS){
        panic("kernel too large\n");
    }
    app_info_ptr=(uint64 *)_app_num;// 由scripts/pack.py生成的os/link_app.S文件中的一个地址名
    app_cur=-1;//无app运行
    app_num=*app_info_ptr;
}

__attribute__((aligned(4096))) char user_stack[USER_STACK_SIZE];
__attribute__((aligned(4096))) char trap_page[TRAP_PAGE_SIZE];
//user_stack 和 trap_page 在内存中的地址按照 4096 字节的倍数对齐


//os内核读取link_app.S的info并把它们搬运到0x80400000开始位置的具体过程。
int load_app(uint64 *info)
{   
	uint64 start = info[0], end = info[1], length = end - start;
	memset((void *)BASE_ADDRESS, 0, MAX_APP_SIZE);  //先将app要加载区域初始化为0
	memmove((void *)BASE_ADDRESS, (void *)start, length);//将app拷贝到BASE_ADDRESS
	return length;//确认长度
}



int run_next_app()
{
	struct trapframe *trapframe = (struct trapframe *)trap_page;
	app_cur++;
	app_info_ptr++;
	if (app_cur >= app_num) {
		return -1;
	}
	infof("load and run app %d", app_cur);
	//uint64 length = 
	load_app(app_info_ptr);
	//debugf("bin range = [%p, %p)", *app_info_ptr, *app_info_ptr + length);
	memset(trapframe, 0, 4096);//清空trap结构体
	trapframe->epc = BASE_ADDRESS;//执行位置
	trapframe->sp = (uint64)user_stack + USER_STACK_SIZE;//用户栈栈顶

	usertrapret(trapframe, (uint64)boot_stack_top);
	//用户态发生中断时，需要把中断上下文结构体 trapframe 保存到内核栈
	//通过 usertrapret 函数进入app的用户层执行
	return 0;
}