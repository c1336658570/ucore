#include "queue.h"
#include "defs.h"

//初始化队列
void init_queue(struct queue *q)
{
	q->front = q->tail = 0;
	q->empty = 1;
}

void push_queue(struct queue *q, int value)
{
	//队列非空，且front和tail保存同一个地址
	if (!q->empty && q->front == q->tail) {
		panic("queue shouldn't be overflow");
	}
	q->empty = 0;	//将队列设置为非空
	q->data[q->tail] = value;	//把value添加到队尾		即将一个进程的pcb相对于pool的偏移放入队尾
	q->tail = (q->tail + 1) % NPROC;	//更新队尾指针
}

int pop_queue(struct queue *q)
{
	//队列空
	if (q->empty)
		return -1;
	//取出队头
	int value = q->data[q->front];
	//更新对头
	q->front = (q->front + 1) % NPROC;
	//对头等于队尾即队列空
	if (q->front == q->tail)
		q->empty = 1;
	return value;
}
