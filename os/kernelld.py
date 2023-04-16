#内核链接脚本
'''
这个脚本也会遍历../user/target/，并对每一个bin文件分配对齐的空间。
最终修改后的kernel_app.ld脚本中多了如下对齐要求:
.data : {
    *(.data)
    . = ALIGN(0x1000);
    *(.data.app0)
    . = ALIGN(0x1000);
    *(.data.app1)
    . = ALIGN(0x1000);
    *(.data.app2)
    . = ALIGN(0x1000);
    *(.data.app3)
    . = ALIGN(0x1000);
    *(.data.app4)

    *(.data.*)
}
'''
import os

TARGET_DIR = "../user/target/"

if __name__ == '__main__':
    f = open("kernel_app.ld", mode="w")
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

