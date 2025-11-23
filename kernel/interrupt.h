#ifndef __KERNEL_INTERRUPT_H__
#define __KERNEL_INTERRUPT_H__
typedef void * intr_handler;
#include "stdint.h"
void idt_init();

// 定义开关中断的两种状态
enum intr_status
{
    INTR_OFF,
    INTR_ON
};

enum intr_status intr_get_status();
enum intr_status intr_enable();
enum intr_status intr_disable();
enum intr_status intr_set_intr(enum intr_status status);
void register_handler(uint8_t vector_no,intr_handler function);
#endif