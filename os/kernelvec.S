	#
        # interrupts and exceptions while in supervisor
        # mode come here.
        #
        # push all registers, call kerneltrap(), restore, return.
        #
        #当处于S模式时，中断和异常会跳转到这里。

        #保存所有寄存器，调用 kerneltrap() 函数，然后恢复寄存器并返回。
.globl kerneltrap
.globl kernelvec
.align 4
kernelvec:
        // make room to save registers.
        //腾出空间来保存寄存器。
        addi sp, sp, -256
        // save the registers.
        //保存寄存器。
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

	// call the C trap handler in trap.c
        //调用 trap.c 中的 C 陷阱处理程序。
        call kerneltrap

kernelret:
        // restore registers.
        //恢复寄存器。
        // 思考：为什么直接就使用了sp？
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        // not this, in case we moved CPUs: ld tp, 24(sp)
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        addi sp, sp, 256

        // return to whatever we were doing in the kernel.
        //返回我们在内核中所做的任何事情。
        sret
        #特权级别由 S（Supervisor）切换到用户态，实现从异常处理程序中返回到用户程序。
        #当 sret 指令被执行时，当前的 SP（栈指针）和 PC（程序计数器）会被自动保存到 SP 和 SCAUSE 寄存器中，然后 SIE（Supervisor Interrupt Enable）寄存器会被设置为 SPP（Supervisor Previous Privilege）标记的值，错位寄存器 SRW 中的 RW 位也会被设置为 SPV（S 态版本）。接着，sret 指令会使系统跳转到保存在 SRET 寄存器中的地址，同时设置新的特权级别和相关状态信息，并开始执行在用户态下的程序。因此，sret 指令的功能类似于 RISC-V 架构中的 eret 指令，但它只涉及到 S 态的切换。