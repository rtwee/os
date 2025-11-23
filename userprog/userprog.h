#ifndef __USERPROG_USERPROG_H__
#define __USERPROG_USERPROG_H__
//用户空间栈的虚拟地址，因为就申请了一页，我们拿栈顶，那就是内核空间下面那块
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)
#define USER_VADDR_START 0x8048000  //用户的虚拟起始地址
#define default_prio 31
#endif