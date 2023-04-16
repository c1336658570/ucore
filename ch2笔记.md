# ch2

当应用程序调用标准C库函数时，标准C库函数在user/lib中，如stdio.c、string.c等，stdio.c、string.c等文件中的标准C库调用ucore的系统调用，ucore的系统调用在user/lib/syscall.c中定义，user/lib/syscall.c中的系统调用函数会调用syscall，syscall是一个宏定义在user/lib/syscall.h中。user/lib/syscall.h中有几个宏，来获取系统调用名。如果在user/lib/syscall.c中调用syscall传递四个参数，最后就会转为调用__ syscall3。如果调用user/lib/syscall.c中调用syscall传递三个参数，最后就会转为调用__syscall2。

__ syscall1，__ syscall2，__ syscall3等函数定义在user/lib/arch/riscv/syscall_arch.h中，在__ syscall1、__ syscall2等函数中使用了ecall（异常的一种），会触发异常。然后执行异常处理函数（异常处理函数的地址在stvec中保存），即执行os/trampoline.S的uservec函数，该函数先将用户程序的各个寄存器保存，然后跳转到os/trap.c的usertrap函数，usertrap函数先读取sstatus寄存器，判断异常或中断原因，如果是系统调用就执行系统调用，然后恢复用户程序的寄存器，如果是其他异常，就直接去加载下一个应用程序，然后执行下一个应用程序。

U态进行ecall调用具体的异常编号是8-Environment call from U-mode。RISCV处理异常需要引入几个特殊的寄存器——CSR寄存器。

S态的CSR寄存器：

- scause: 它用于记录异常和中断的原因。它的最高位为1是中断，否则是异常。其低位决定具体的种类。
- sepc：处理完毕中断异常之后需要返回的PC值。
- stval: 产生异常的指令的地址。
- stvec：处理异常的函数的起始地址。
- sstatus：记录一些比较重要的状态，比如是否允许中断异常嵌套。

需要注意的是这些寄存器是S态的CSR寄存器。M态还有一套自己的CSR寄存器mcause，mtvec…

U态执行ecall指令的时候就产生了异常。此时CPU会处理上述的各个CSR寄存器，之后跳转至stvec所指向的地址，也就是我们的异常处理函数。os的这个函数的具体位置是在trap_init函数（os/trap.c）之中就指定了——是uservec函数。这个函数位于os/trampoline.S之中，是由汇编语言编写的。在uservec之中，os保存了U态执行流的各个寄存器的值。这些值的位置其实已经由trap.h（os/trap.h）中的trapframe结构体规定好了。

```c
// os/trap.h
struct trapframe {
    /*   0 */ uint64 kernel_satp;   // kernel page table
    /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
    /*  16 */ uint64 kernel_trap;   // usertrap entry
    /*  24 */ uint64 epc;           // saved user program counter
    /*  32 */ uint64 kernel_hartid; // saved kernel tp， unused in our project
    /*  40 */ uint64 ra;
    /*  48 */ uint64 sp;
    /* ... */ ....
    /* 272 */ uint64 t5;
    /* 280 */ uint64 t6;
};
```

```assembly
#os/trampoline.S
.section .text
.globl trampoline
trampoline:
.align 4
.globl uservec
uservec:        #os保存了U态执行流的各个寄存器的值。
	#
        # trap.c sets stvec to point here, so
        # traps from user space start here,
        # in supervisor mode, but with a
        # user page table.
        #
        # sscratch points to where the process's p->trapframe is
        # mapped into user space, at TRAPFRAME.
        #

	# swap a0 and sscratch
        # so that a0 is TRAPFRAME
        csrrw a0, sscratch, a0          #交换a0和sscrath
        #sscratch这个CSR寄存器的作用就是一个cache，它只负责存某一个值，这里它保存的就是TRAPFRAME结构体的位置。
        # save the user registers in TRAPFRAME

        #保存ra-t6（a0除外）
        sd ra, 40(a0)
        sd sp, 48(a0)
        sd gp, 56(a0)
        sd tp, 64(a0)
        sd t0, 72(a0)
        sd t1, 80(a0)
        sd t2, 88(a0)
        sd s0, 96(a0)
        sd s1, 104(a0)
        sd a1, 120(a0)
        sd a2, 128(a0)
        sd a3, 136(a0)
        sd a4, 144(a0)
        sd a5, 152(a0)
        sd a6, 160(a0)
        sd a7, 168(a0)
        sd s2, 176(a0)
        sd s3, 184(a0)
        sd s4, 192(a0)
        sd s5, 200(a0)
        sd s6, 208(a0)
        sd s7, 216(a0)
        sd s8, 224(a0)
        sd s9, 232(a0)
        sd s10, 240(a0)
        sd s11, 248(a0)
        sd t3, 256(a0)
        sd t4, 264(a0)
        sd t5, 272(a0)
        sd t6, 280(a0)

	# save the user a0 in p->trapframe->a0
        csrr t0, sscratch
        sd t0, 112(a0)  #保存a0

        csrr t1, sepc
        sd t1, 24(a0)   #保存用户程序计数器

        ld sp, 8(a0)    #将进程内核栈顶部加载到sp
        ld tp, 32(a0)   #将内核tp加载到tp
        ld t1, 0(a0)    #将内核页表加载到t1
        # csrw satp, t1
        # sfence.vma zero, zero
        ld t0, 16(a0)   #kernel_trap加载到t0就是函数usertrap的地址
        jr t0 #jr t0,就跳转到了我们早先设定在 trapframe->kernel_trap 中的地址，也就是 trap.c 之中的 usertrap 函数。这个函数在main的初始化之中已经调用了。
```

sscratch这个CSR寄存器的作用就是一个cache，它只负责存某一个值，这里它保存的就是TRAPFRAME结构体的位置。csrr和csrrw指令是RV特供的读写CSR寄存器的指令。我们取用它的值的时候实际把原来a0的值和其交换了，因此返回时大家可以看到我们会再交换一次得到原来的a0。这里注释了两句代码大家可以不用管，这是页表相关的处理，我们在ch4会仔细了解它。

然后我们使用jr t0,就跳转到了我们早先设定在 trapframe->kernel_trap 中的地址，也就是 trap.c 之中的 usertrap 函数。这个函数在main的初始化之中已经调用了。

```c
// os/trap.c
// set up to take exceptions and traps while in the kernel.
void trap_init(void)
{
	w_stvec((uint64)uservec & ~0x3);	//userver是在trampoline.S中定义的函数，写 stvec, 最后两位表明跳转模式，该实验始终为 0
}
```

该函数完成异常中断处理与返回，包括执行我们写好的syscall。

从S态返回U态是由 usertrapret 函数实现的。这里设置了返回地址sepc，并调用另外一个 userret 汇编函数来恢复 trapframe 结构体之中的保存的U态执行流数据。

```c
//os/trap.c
void usertrapret(struct trapframe *trapframe, uint64 kstack)	//从S态返回U态
{
	//这里设置了返回地址sepc，并调用另外一个 userret 汇编函数来恢复 trapframe 结构体之中的保存的U态执行流数据。
	trapframe->kernel_satp = r_satp(); // kernel page table
	trapframe->kernel_sp = kstack + PGSIZE; // process's kernel stack
	trapframe->kernel_trap = (uint64)usertrap;
	trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

	w_sepc(trapframe->epc);	// 设置了sepc寄存器的值。
	// set up the registers that trampoline.S's sret will use
	// to get to user space.

	// set S Previous Privilege mode to User.
	uint64 x = r_sstatus();
	x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
	x |= SSTATUS_SPIE; // enable interrupts in user mode
	w_sstatus(x);

	// tell trampoline.S the user page table to switch to.
	// uint64 satp = MAKE_SATP(p->pagetable);
	userret((uint64)trapframe);	//定义在trampoline.S中 恢复 trapframe 结构体之中的保存的U态执行流数据。
}
```

同样由于涉及寄存器的恢复，以及未来页表satp寄存器的设置等，userret也必须是一个汇编函数。它基本上就是uservec函数的镜像，将保存在trapframe之中的数据依次读出用于恢复对应的寄存器，实现恢复用户中断前的状态。

```assembly
#os/trampoline.S
.globl userret
userret:
        # userret(TRAPFRAME, pagetable)
        # switch from kernel to user.
        # usertrapret() calls here.
        # a0: TRAPFRAME, in user page table.
        # a1: user page table, for satp.

        # switch to the user page table.
        # csrw satp, a1
        # sfence.vma zero, zero

        # put the saved user a0 in sscratch, so we
        # can swap it with our a0 (TRAPFRAME) in the last step.
        ld t0, 112(a0)
        csrw sscratch, t0

        # restore all but a0 from TRAPFRAME
        ld ra, 40(a0)
        ld sp, 48(a0)
        ld gp, 56(a0)
        ld tp, 64(a0)
        ld t0, 72(a0)
        ld t1, 80(a0)
        ld t2, 88(a0)
        ld s0, 96(a0)
        ld s1, 104(a0)
        ld a1, 120(a0)
        ld a2, 128(a0)
        ld a3, 136(a0)
        ld a4, 144(a0)
        ld a5, 152(a0)
        ld a6, 160(a0)
        ld a7, 168(a0)
        ld s2, 176(a0)
        ld s3, 184(a0)
        ld s4, 192(a0)
        ld s5, 200(a0)
        ld s6, 208(a0)
        ld s7, 216(a0)
        ld s8, 224(a0)
        ld s9, 232(a0)
        ld s10, 240(a0)
        ld s11, 248(a0)
        ld t3, 256(a0)
        ld t4, 264(a0)
        ld t5, 272(a0)
        ld t6, 280(a0)

	# restore user a0, and save TRAPFRAME in sscratch
        csrrw a0, sscratch, a0

        # return to user mode and user pc.
        # usertrapret() set up sstatus and sepc.
        sret
  		#sret指令执行了2个事情：从S态回到U态，并将PC移动到sepc指定的位置，继续执行用户程序。
```

# 实现批处理操作系统的细节

## 本节导读

前面一节中我们明白了os是如何执行应用程序的。但是os是如何”找到“这些应用程序并允许它们的呢？在引言之中我们简要介绍了这是由link_app.S以及kernel_app.ld完成的。实际上，能够在批处理操作系统与应用程序之间建立联系的纽带。这主要包括两个方面：

- 静态编码：通过一定的编程技巧，把应用程序代码和批处理操作系统代码“绑定”在一起。
- 动态加载：基于静态编码留下的“绑定”信息，操作系统可以找到应用程序文件二进制代码的起始地址和长度，并能加载到内存中运行。

这里与硬件相关且比较困难的地方是如何让在内核态的批处理操作系统启动应用程序，且能让应用程序在用户态正常执行。

## 将应用程序链接到内核

### makefile更新

我们首先看一看本章的makefile改变了什么

```makefile
link_app.o: link_app.S
link_app.S: pack.py
    @$(PY) pack.py
kernel_app.ld: kernelld.py
    @$(PY) kernelld.py

kernel: $(OBJS) kernel_app.ld link_app.S
    $(LD) $(LDFLAGS) -T kernel_app.ld -o kernel $(OBJS)
    $(OBJDUMP) -S kernel > kernel.asm
    $(OBJDUMP) -t kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernel.sym
```

可以看到makefile执行了两个python脚本生成了我们提到的link_app.S和kernel_app.ld。这里选择python只是因为比较好写生成的代码，我们的os和python没有任何关系。link_app.S的大致内容如下:

```assembly
    .align 4
    .section .data
    .global _app_num
_app_num:
    .quad 2
    .quad app_0_start
    .quad app_1_start
    .quad app_1_end

    .global _app_names
_app_names:
.string "hello.bin"
.string "matrix.bin"

    .section .data.app0
    .global app_0_start
app_0_start:
    .incbin "../user/target/bin/ch2t_write0.bin"

    .section .data.app1
    .global app_1_start
app_1_start:
    .incbin "../user/target/bin/ch2b_write1.bin"
app_1_end:
```

pack.py会遍历../user/target/bin，并将该目录下的目标用户程序*.bin包含入 link_app.S中，同时给每一个bin文件记录其地址和名称信息。最后，我们在 Makefile 中会将内核与 link_app.S 一同编译并链接。这样，我们在内核中就可以通过 extern 指令访问到用户程序的所有信息，如其文件名等。

由于 riscv 要求程序指令必须是对齐的，我们对内核链接脚本也作出修改，保证用户程序链接时的指令对齐，这些内容见 os/kernelld.py。这个脚本也会遍历../user/target/，并对每一个bin文件分配对齐的空间。最终修改后的kernel_app.ld脚本中多了如下对齐要求:

```ld
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
```

编译出的kernel已经包含了bin文件的信息。熟悉汇编的同学可以去看看生成的kernel.asm（kernel整体的汇编代码）来加深理解。

### 内核的relocation

内核中通过访问 link_app.S 中定义的 _app_num、app_0_start 等符号来获得用户程序位置.

```c
// os/loader.c
extern char _app_num[]; // 在link_app.S之中已经定义
void batchinit() {
    app_info_ptr = (uint64*) _app_num;
    app_num = *app_info_ptr;
    app_info_ptr++;
    // from now on:
    // app_n_start = app_info_ptr[n]
    // app_n_end = app_info_ptr[n+1]
}
```

然而我们并不能直接跳转到 app_n_start 直接运行，因为用户程序在编译的时候，会假定程序处在虚存的特定位置，而由于我们还没有虚存机制，因此我们在运行之前还需要将用户程序加载到规定的物理内存位置。为此我们规定了用户的链接脚本，并在内核完成程序的 “搬运”

```ld
# user/lib/arch/riscv/user.ld
SECTIONS {
    . = 0x80400000;                 #　规定了内存加载位置

    .startup : {
        *crt.S.o(.text)             #　确保程序入口在程序开头
    }

    .text : { *(.text) }
    .data : { *(.data .rodata) }

    /DISCARD/ : { *(.eh_*) }
}
```

这样之后，我们就可以在读取指定内存位置的bin文件来执行它们了。下面是os内核读取link_app.S的info并把它们搬运到0x80400000开始位置的具体过程。

```c
// os/loader.c
const uint64 BASE_ADDRESS = 0x80400000, MAX_APP_SIZE = 0x20000;
int load_app(uint64* info) {
    uint64 start = info[0], end = info[1], length = end - start;
    memset((void*)BASE_ADDRESS, 0, MAX_APP_SIZE);
    memmove((void*)BASE_ADDRESS, (void*)start, length);
    return length;
}
```

## 用户栈与内核栈

我们自己的OS内核运行时，是需要一个栈来存放自己需要的变量的，这个栈我们称之为内核栈。在RV之中，我们使用sp寄存器来记录当前栈顶的位置。因此，在进入OS之前，我们需要告诉qemu我们OS的内核栈的起始位置。这个在entry.S之中有实现:

```assembly
// entry.S
 _entry:
    la sp, boot_stack_top
    call main

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 * 16
    .globl boot_stack_top
```

一个应用程序肯定也需要内存空间来存放执行时需要的种种变量（实际上就是执行程序对应的用户栈），同时我们在上一章节提到了trapframe，这个也需要一个空间存放。那么OS是如何给应用程序分配这些对应的空间的呢？

实际上，我们采用一个静态分配的方式来给程序分配对应的一定大小的空间,并在run_next_app函数初始化应用程序对应的trapframe，并将用户栈对应的起始位置写入trapframe之中的sp寄存器，来让程序找到自己用户栈起始的位置。（注意栈在空间是高到低位，因此这里起始位置的初始化是在静态分配数组的尾部)。

```c.
// loader.c
__attribute__((aligned(4096))) char user_stack[USER_STACK_SIZE];
__attribute__((aligned(4096))) char trap_page[TRAP_PAGE_SIZE];

int run_next_app()
{
    struct trapframe *trapframe = (struct trapframe *)trap_page;
    ...
    memset(trapframe, 0, 4096);
    trapframe->epc = BASE_ADDRESS;
    trapframe->sp = (uint64)user_stack + USER_STACK_SIZE;
    usertrapret(trapframe, (uint64)boot_stack_top);
    ...
}
```
