#生成os/kernel_app.ld，取代之前的kernel.ld。编译器会把把操作系统的源码和os/link_app.S合在一起，
#编译出操作系统+Binary应用的ELF执行文件，并进一步转变成Binary格式。
'''
由于 riscv 要求程序指令必须是对齐的，我们对内核链接脚本也作出修改，保证用户程序链接时的指令对齐，
这些内容见 os/kernelld.py。这个脚本也会遍历../user/target/，
并对每一个bin文件分配对齐的空间。最终修改后的kernel_app.ld脚本中多了对齐要求：. = ALIGN(0x1000);
'''

import os

TARGET_DIR = "./user/target/bin/"

if __name__ == '__main__':
    f = open("os/kernel_app.ld", mode="w")
    apps = os.listdir(TARGET_DIR)
    f.write(
'''OUTPUT_ARCH(riscv)
ENTRY(_entry)
BASE_ADDRESS = 0x80200000;

SECTIONS
{
    . = BASE_ADDRESS;
    skernel = .;

    s_text = .;
    .text : {
        *(.text.entry)
        *(.text .text.*)
        . = ALIGN(0x1000);
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
        *(.data)
''')
    for (idx, _) in enumerate(apps):
        f.write('        . = ALIGN(0x1000);\n')
        f.write('        *(.data.app{})\n'.format(idx))
    f.write(
'''
        . = ALIGN(0x1000);
        *(.data.*)
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
''')
    f.close()

