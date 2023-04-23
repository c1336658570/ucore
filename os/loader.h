#ifndef LOADER_H
#define LOADER_H

#include "const.h"
#include "types.h"

int finished();
void loader_init();
int run_all_app();

#define BASE_ADDRESS (0x80400000) //第一个用户程序加载的地址
#define MAX_APP_SIZE (0x20000)    //每一个用户程序的最大大小
#define USER_STACK_SIZE (PAGE_SIZE)   //用户栈大小
#define TRAP_PAGE_SIZE (PAGE_SIZE)

#endif // LOADER_H