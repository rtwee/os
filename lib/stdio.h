#ifndef __LIB_STDIO_H__
#define __LIB_STDIO_H__

#include "stdint.h"

#define va_start(ap,v) ap=(va_list)&v   //初始化指向v
#define va_arg(ap,t) *((t*)(ap+=4))     //指向下一个参数并返回值
#define va_end(ap)     ap=NULL          //清除ap

typedef void* va_list;
uint32_t printf(const char * format,...);
uint32_t vsprintf(char * str,const char * format,va_list ap);
#endif