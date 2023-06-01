//新增，文件相关的一些抽象
#ifndef FCNTL_H
#define FCNTL_H

#define O_RDONLY 0x000    //只读
#define O_WRONLY 0x001    //只写
#define O_RDWR 0x002      //读写
#define O_CREATE 0x200    //找不到该文件的时候创建文件
#define O_TRUNC 0x400     //打开文件的时候清空文件内容并将文件大小归0

#endif // FCNIL_H