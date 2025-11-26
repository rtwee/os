#ifndef __LIB_KERNEL_STDIO_KERNEL_H__
#define __LIB_KERNEL_STDIO_KERNEL_H__
#include "../../kernel/global.h"
#include "../stdio.h"

// typedef char* va_list;
// #define va_start(ap,v) ap = (va_list)&v     //把ap指向一个固定参数v
// #define va_arg(ap,t) *((t*)(ap += 4))       //ap指向下一个参数并返回其值
// #define va_end(ap) ap = NULL;               //清楚ap

void printk(const char * format,...);

#endif