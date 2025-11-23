#ifndef __LIB_KERNEL_IO_H__
#define __LIB_KERNEL_IO_H__
#include "../stdint.h"
//b 一字节
//w 一个字

//      data输出到 dx中 dx中的内容是port
static inline void outb(uint16_t port,uint8_t data)
{
    asm volatile("outb %b0,%w1"::"a"(data),"Nd"(port));
}

//      addr处起始的word_cnt 个 字（2个字节） 写入端口port ;;;;我们的 es ds 都是一样的
static inline void outsw(uint16_t port,const void * addr,uint32_t word_cnt)
{
    //  outsw指令  把 ds ：esi处的 16位 内容 写入 dx 中
    asm volatile ("cld;rep outsw":"+S"(addr),"+c"(word_cnt):"d"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t data;
    asm volatile ("inb %w1,%b0":"=a"(data):"Nd"(port));
    return data;
}

//      将port读出来的 字（16位）写入到 es:edi  ;;;;我们的 es ds 都是一样的
static inline void insw(uint16_t port,void * addr,uint32_t word_cnt)
{
    asm volatile ("cld;rep insw":"+D"(addr),"+c"(word_cnt):"d"(port):"memory");
}

#endif