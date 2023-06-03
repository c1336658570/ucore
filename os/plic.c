//新增，用来处理磁盘中断

#include "plic.h"
#include "log.h"
#include "proc.h"
#include "riscv.h"
#include "types.h"

// the riscv Platform Level Interrupt Controller (PLIC).
//riscv 平台级中断控制器 (PLIC)。

//用于配置平台级中断控制器（PLIC），以便定位和处理发生的中断事件。
void plicinit()
{
	// set desired IRQ priorities non-zero (otherwise disabled).
	//将所需的中断请求优先级设置为非零，否则将会禁用中断 
	int hart = cpuid();	//获取本地 CPU 的 ID，以方便配置对应的中断控制寄存器。
	// 将 VIRTIO0_IRQ 指定的 VIRTIO 设备中断的优先级设置为1。
	*(uint32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;	//将 VIRTIO0_IRQ 指定的 VIRTIO 块设备中断的优先级设置为非零（默认为0将导致该中断被禁用）。
	// set uart's enable bit for this hart's S-mode.
	//将 S-模式的中断控制寄存器 SENABLE 的相应位设置为1，以启用 VIRTIO0_IRQ 中断。
	*(uint32 *)PLIC_SENABLE(hart) = (1 << VIRTIO0_IRQ);	//将此 CPU 核心（hart）的 S-模式的中断控制寄存器 SENABLE 的相应位设置为1，以启用 VIRTIO0_IRQ 中断。
	// set this hart's S-mode priority threshold to 0.
	//设置此 CPU 核心（hart）的中断优先级阈值寄存器 SPRIORITY 的值为0，以允许所有的 S-模式的中断请求被接受。
	*(uint32 *)PLIC_SPRIORITY(hart) = 0;	//将 S-模式的 PLIC 优先级阈值寄存器 SPRIORITY 的值设置为0，以允许所有的 S-模式的中断请求被接受。
}

// ask the PLIC what interrupt we should serve.
//用于查询平台级中断控制器（PLIC）中未处理的中断服务请求，并返回对应的中断号。
/*
函数调用过程如下：
首先使用 cpuid() 函数获取本地 CPU 的 ID 号。
使用 PLIC_SCLAIM(hart) 宏查询当前 CPU（hart）S-模式下的中断响应请求寄存器，以确定是否有未处理的中断请求。
如果有未处理的中断请求，则返回该请求的中断号；如果没有未处理的中断请求，则返回中断号为0，表示没有可响应的中断请求。
将返回的中断号用于处理对应的中断请求。
*/
int plic_claim()
{
	int hart = cpuid();	//获取本地 CPU 的 ID。
	//查询 S-模式下的 PLIC，在等待服务的中断请求中选择一个未被处理的请求，并返回该请求的中断号。
	int irq = *(uint32 *)PLIC_SCLAIM(hart);	
	//如果没有未处理的中断请求，则返回中断号为0。
	return irq;
}
/*
注解：
“plic_claim()”：函数名，用于在平台级中断控制器中查询获取一个未处理的中断服务请求，并返回对应的中断号。
“cpuid()”：函数调用，用于获取本地 CPU 的 ID 号。
“PLIC_SCLAIM(hart)”：用于访问当前 CPU（hart）S-模式的中断控制寄存器 SCLAIM，用于查询等待服务的中断请求数量。
“int irq = *(uint32 *)PLIC_SCLAIM(hart)”: 将查询到的中断请求号存储在变量 irq 中以备后续处理。
返回中断请求号，用于处理对应的中断请求。
*/


// tell the PLIC we've served this IRQ.
/*
函数调用过程如下：
首先使用 cpuid() 函数获取本地 CPU 的 ID 号。
使用参数 irq，来向 S-模式下的中断控制寄存器 SCLAIM 发送完成中断响应的消息，告诉 PLIC 已经成功处理了该中断服务请求。
*/
//告诉平台级中断控制器（PLIC）已经处理完了指定的中断服务请求。
void plic_complete(int irq)
{
	int hart = cpuid();	//获取本地 CPU 的 ID。
	//调用 PLIC_SCLAIM(hart) 宏，告诉当前 CPU 的 S-模式下的 PLIC，已经成功处理了指定的中断号参数所请求的中断服务请求。
	*(uint32 *)PLIC_SCLAIM(hart) = irq;
}
/*
说明：
“plic_complete(int irq)”：函数名，用于通知平台级中断控制器已经成功处理了指定的中断服务请求。
“*(uint32 *)PLIC_SCLAIM(hart) = irq”：调用宏 PLIC_SCLAIM(hart)，它实际上是将该参数 irq 传递给当前 
CPU（hart）S-模式的中断控制寄存器 SCLAIM 来完成中断响应通信。此时 IRQ 可以被视为已被处理，
稍后使用 plic_claim() 来查询下一个待处理的中断服务请求。
只有当指定中断号 irq 已处理的情况下，才应该调用plic_complete()函数来完成中断请求的处理。
*/