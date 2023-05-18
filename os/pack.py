#生成os/link_app.S，内核中通过访问 link_app.S 中定义的 _app_num、app_0_start 等符号来获得用户程序位置.
'''
pack.py会遍历../user/target/bin，并将该目录下的目标用户程序*.bin包含入
link_app.S中，同时给每一个bin文件记录其地址和名称信息。
最后，我们在 Makefile 中会将内核与 link_app.S 一同编译并链接。
这样，我们在内核中就可以通过 extern 指令访问到用户程序的所有信息，如其文件名等。
'''

'''
操作系统本身需要完成对Binary应用的位置查找，找到后（通过os/link_app.S中的变量和标号信息完成），
会把Binary应用拷贝到os/kernel_app.ld 指定的物理内存位置（OS的加载应用功能）。
'''

import os

TARGET_DIR = "./user/target/bin/"

if __name__ == '__main__':
    f = open("os/link_app.S", mode="w")
    apps = os.listdir(TARGET_DIR)
    apps.sort()
    f.write(
'''    .align 4
    .section .data
    .global _app_num
_app_num:
    .quad {}
'''.format(len(apps))   #.quad是一个伪指令，用于声明一个8字节（64位）的整数。
    )

    for (idx, _) in enumerate(apps):
        f.write('    .quad app_{}_start\n'.format(idx))
    f.write('    .quad app_{}_end\n'.format(len(apps) - 1))

    f.write(
'''
    .global _app_names
_app_names:
''');

    for app in apps:
        f.write("   .string \"" + app + "\"\n") #.string是一个伪指令，用于声明一个字符串。它会按照ASCII码格式将字符串转换成一个以空字符 '\0' 结尾的字符数组。

    for (idx, app) in enumerate(apps):
        f.write(
'''
    .section .data.app{0}
    .global app_{0}_start
app_{0}_start:
    .incbin "{1}"   
'''.format(idx, TARGET_DIR + app)   #.incbin是一个伪指令，用于将二进制文件包含到汇编程序中。
        )
    f.write('app_{}_end:\n\n'.format(len(apps) - 1))
    f.close()