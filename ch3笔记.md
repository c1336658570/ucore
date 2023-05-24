# ch3笔记

## 从main开始执行

1. main函数调用clean_bss()函数先清空内核的bss段
2. 调用proc_init()初始化进程池，在该函数里，将所有进程的状态置为UNUSED（未使用）状态。然后为每一个进程分配用户态栈，内核态栈还有trapframe。并设置idel进程的内核栈，给idel分配进程id（0），让指向当前执行进程的指针指向idel进程。
3. 调用loader_init()函数，初始化app_num，让app_num保存用户程序的数量，初始化app_info_ptr，让该指针指向第一个用户程序。
4. 调用trap_init()，设置stvec寄存器，该寄存器保存了中断发生时的函数地址。
5. 调用timer_init()开启时中中断
6. 调用run_all_app()将所有用户程序加载到指定位置，此函数开头调用allocproc()函数，allocproc()函数会分配一个进程，而且在这个函数里面会给进程分配进程号，修改进程状态为USED，而且会将context，trapframe和内核栈清零，并设置p->context.ra为usertrapret和p->context.sp=p->kstack+PAGE_SIZE  。设置的这两个参数将在第8步调度时使用。
7. 而且设置了每个进程的trapframe-epc为进程入口地址，trapframe->sp为每个进程的用户栈地址，并将每个进程状态设置为RUNNABLE（就绪态）。
8. 调用scheduler开始调度。在这个函数里调用了swtch()，swtch负责将正在执行的进程的被调用者保存寄存器保存到context中，然后将即将要执行的进程的context加载到保存寄存器中（其中包括第6布设置的ra和sp），最后调用ret，然后就会返回到ra所指向的那个位置开始执行，即usertrapret这个函数这个函数会设置tramframe的值，然后通过一个汇编函数，跳转到用户程序开始执行

context.ra和context.sp只在proc.c的allocproc函数执行时设置了，以后就再也没有修改过，所以每次调用完switch后都会进入到usertrapret

当main调用scheduler时，然后在scheduler会第一次调用switch，该函数会将当前执行流（main函数）的ra，sp等寄存器保存到idel.context中，然后将p.context中保存的内容加载到ra，sp等寄存器中。然后通过ret这条指令，返回到（ra寄存器保存的那个地址开始执行）usertrapret开始执行，在这里会将当前进程的trapframe结构体的元素进行修改，然后调用userret，将trapframe结构体保存的东西加载到寄存器中（sscratch等寄存器），sscratch存的是用户程序trapframe结构体的地址。开始执行用户程序。当用户程序发生中断时，会进入uservec函数，然后将当前执行流的所有寄存器保存到sscratch寄存器所指向的那块地址，然后跳到usertrap执行，在这个函数里进行判断，如果是系统调用就执行系统调用，如果是时中中断就调用yield()，yield函数会调用switch函数将当前执行流的ra，sp等寄存器保存到p.context中，然后将idel.context加载到ra，sp等寄存器中，然后通过ret返回到上次从main函数切换的地方开始执行。