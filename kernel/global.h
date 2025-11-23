#ifndef __KERNEL_GLOBAL_H__
#define __KERNEL_GLOBAL_H__

#include "../lib/stdint.h"

#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define TI_GDT  0
#define TI_LDT  1

//-------------------------IDT描述符属性---------------------------
#define IDT_DESC_P 1
#define IDT_DESC_DPL0 0
#define IDT_DESC_DPL3 3
#define IDT_DESC_32_TYPE 0xe
#define IDT_DESC_16_TYPE 0x6



#define IDT_DESC_ATTR_DPL0  ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3  ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)

#define STACK_MAGIC 0x20000319

// -------------------------- GDT描述符属性 --------------------------
#define DESC_G_4K   1 //单位是4K
#define DESC_D_32   1 //寄存器大小是32位
#define DESC_L      0 //64位的代码标记
#define DESC_AVL    0 //cpu不用此位
#define DESC_P      1 //在内存中
#define DESC_DPL_0  0 
#define DESC_DPL_1  1 
#define DESC_DPL_2  2 
#define DESC_DPL_3  3

// 代码段 和 数据段 同属于存储段，tss和各种任务们属于系统段
// s 1表示存储段  0 表示系统段
#define DESC_S_CODE 1
#define DESC_S_DATA DESC_S_CODE
#define DESC_S_SYS  0
#define DESC_TYPE_CODE 8    //表示属性可执行，一致性还有个保留位;x=1,c=0,r=0,a=0
#define DESC_TYPE_DATA 2    //;x=0,e=0,w=1,a=0 可写
#define DESC_TYPE_TSS  9    //B位取0，不忙

#define SELECTOR_K_CODE ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK SELECTOR_K_DATA
#define SELECTOR_K_GS ((3 << 3) + (TI_GDT << 2) + RPL0)
//第四个段描述符是TSS
#define SELECTOR_U_CODE ((5 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_DATA ((6 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_STACK SELECTOR_U_DATA

//一些属性
#define GDT_ATTR_HIGH ((DESC_G_4K << 7) + (DESC_D_32 << 6) + (DESC_L << 5) + (DESC_AVL << 4))            //TSS选择子的20-23位
#define GDT_CODE_ATTR_LOW_DPL3 ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_CODE << 4) + DESC_TYPE_CODE)
#define GDT_DATA_ATTR_LOW_DPL3 ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_DATA << 4) + DESC_TYPE_DATA) //TSS选择子的8-15位

//TSS描述符属性
#define TSS_DESC_D 0
#define TSS_ATTR_HIGH ((DESC_G_4K << 7) + (TSS_DESC_D << 6) + (DESC_L << 5) + (DESC_AVL << 4) + 0x0) 
#define TSS_ATTR_LOW  ((DESC_P << 7) + (DESC_DPL_0 << 5) + (DESC_S_SYS << 4) + DESC_TYPE_TSS)

#define SELECTOR_TSS ((4 << 3) + (TI_GDT << 2) + RPL0)

//定义GDT中的TSS描述符结构
struct gdt_desc
{
    uint16_t limit_low_word;
    uint16_t base_low_word;
    uint8_t  base_mid_byte;
    uint8_t  attr_low_byte;
    uint8_t  limit_high_attr_high;
    uint8_t  base_high_byte;
};


//这里是为用户进程准备的
#define EFLAGS_MBS (1 << 1)     //次项必须要设置
#define EFLAGS_IF_1 (1 << 9)    //if为1开中断
#define EFLAGS_IF_0 0           //if为0关中断
#define EFLAGS_IOPL_3 (3 << 12) //IOPL3,用于测试用户程序在非系统调用下进行IO
#define EFKAGS_IOPL_0 (0 << 12)

#define NULL ((void *)0)
#define DIV_ROUND_UP(X,STEP) ((X + STEP - 1) / (STEP))
#define bool int
#define true 1
#define false 0



#endif
