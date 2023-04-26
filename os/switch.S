# Context switch
#
#   void swtch(struct context *old, struct context *new);
#
# Save current registers in old. Load from new.

#执行流：通过切换 ra
#堆栈：通过切换 sp
#寄存器： 通过保存和恢复被保存寄存器。调用者保存寄存器由编译器生成的代码负责保存和恢复。

#s0-s11为保存寄存器，callee负责保存和恢复，t0-t6为临时寄存器caller负责保存和恢复，
#Caller 需要自己在调用 Callee 之前保存临时寄存器中的值。
.globl swtch
swtch:
        sd ra, 0(a0)            #保存 ra
        sd sp, 8(a0)            #保存 sp
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)            #加载ra 
        ld sp, 8(a1)            #加载sp
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)

        ret     #通过ret切换到ra所执行的那个地址，开始执行