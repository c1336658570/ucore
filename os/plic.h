//新增，用来处理磁盘中断

#ifndef PLIC_H
#define PLIC_H

// qemu puts UART registers here in physical memory.
//qemu 将 UART 寄存器放在物理内存中。
#define UART0 0x10000000L //虚拟串行通信端口的基址，实际位于物理内存地址 0x10000000。
#define UART0_IRQ 10      //虚拟串行通信端口的中断号，其值为 10。

// virtio mmio interface
#define VIRTIO0 0x10001000    //VIRTIO 块设备的内存映射地址，实际位于物理内存地址 0x10001000。
#define VIRTIO0_IRQ 1   //VIRTIO 块设备的中断号，其值为 1。

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L    //平台级中断控制器的基址，实际位于物理内存地址 0x0c000000。
#define PLIC_PRIORITY (PLIC + 0x0)    //平台级中断控制器优先级寄存器的地址。
#define PLIC_PENDING (PLIC + 0x1000)   //平台级中断控制器挂起寄存器的地址。
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100) //指定 CPU 核心 hart 的平台级中断控制器 MENABLE 寄存器地址，用于启用中断。
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100) //指定 CPU 核心 hart 的平台级中断控制器 SENABLE 寄存器地址，用于启用特殊中断。
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)  //指定 CPU 核心 hart 的平台级中断控制器 MPRIORITY 寄存器地址，用于存储中断处理的优先级。
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)  //指定 CPU 核心 hart 的平台级中断控制器 SPRIORITY 寄存器地址，用于存储特殊中断处理的优先级。
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)   //指定 CPU 核心 hart 的平台级中断控制器 MCLAIM 寄存器地址，用于为中断处理程序分配一个唯一的标识符。
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)   //指定 CPU 核心 hart 的平台级中断控制器 SCLAIM 寄存器地址，用于为特殊中断处理程序分配一个唯一的标识符。

void plicinit();
int plic_claim();
void plic_complete(int);

#endif // PLIC_H