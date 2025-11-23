#ifndef __USERPROG_SYSCALL_INIT_H__
#define __USERPROG_SYSCALL_INIT_H__
#include "../lib/stdint.h"
#include "../thread/thread.h"
uint32_t sys_getpid(void);
uint32_t sys_write(char * str);
void syscall_init(void);
#endif