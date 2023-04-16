#设置程序的入口点并初始化引导堆栈    
    .section .text.entry    #该段为内核入口点，放在段最低地址
    .globl _entry           #设定全局
_entry:                     #声明入口点的开始
    la sp, boot_stack_top   #加载地址 (la) 将堆栈指针设置为引导堆栈的顶部
    call main

    .section .bss.stack     #堆栈未初始化数据段
    .globl boot_stack
boot_stack:
    .space 4096 * 16
    .globl boot_stack_top
boot_stack_top:
