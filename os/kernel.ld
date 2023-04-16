/*调整内存布局，链接脚本调整链接器的行为*/
OUTPUT_ARCH(riscv)  
ENTRY(_entry)       /*程序入口点*/
BASE_ADDRESS = 0x80200000;     /*内核初始化代码放置地址*/

/*.text, .rodata .data, .bss 从低地址到高地址按顺序放置*/
SECTIONS
{
    . = BASE_ADDRESS;  /*对.进行赋值来调整接下来的段放在哪里*/
    skernel = .;       /*记录当前地址*/

    s_text = .;   
    .text : {          /*花括号内按照放置顺序描述将所有输入目标文件的哪些段放在这个段中*/
        *(.text.entry)
        *(.text .text.*)/*指定名称以“.text”开头的所有符号。包含在“.text”部分*/
        . = ALIGN(0x1000);/*对齐*/
        *(trampsec)
        . = ALIGN(0x1000);
    }
    . = ALIGN(4K);
    e_text = .;

    s_rodata = .;
    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4K);
    e_rodata = .;

    s_data = .;
    .data : {
        *(.data.apps)
        *(.data .data.*)
        *(.sdata .sdata.*)
    }
    . = ALIGN(4K);
    e_data = .;

    .bss : {
        *(.bss.stack)
        s_bss = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
    }
    . = ALIGN(4K);
    e_bss = .;
    ekernel = .;

    /DISCARD/ : {
        *(.eh_frame)
    }
}