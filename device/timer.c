#include "./timer.h"
#include "io.h"
#include "print.h"
#include "../thread/thread.h"
#include "debug.h"
#include "stdint.h"

#define IRQ0_FREQUENCY      1000
#define INPUT_FREQUENCY     1193180
#define COUNTER0_VALUE       INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT       0x40
#define COUNTER0_NO         0
#define COUNTER_MODE        2
#define READ_WRITE_LATCH    3
#define PIT_CONTROL_PORT    0x43


#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)


// 把计数器counter_no 读写锁存rwl 计数模式 counter_mode 写入控制寄存器 并赋予初值 counter_value
static void frequency_set(uint8_t counter_port,uint8_t counter_no,uint8_t rwl,uint8_t counter_mode,uint16_t counter_value)
{
    outb(PIT_CONTROL_PORT,(uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
    outb(counter_port,(uint8_t)counter_value);
    outb(counter_port,(uint8_t)(counter_value >> 8));
}

uint32_t ticks;//自从中断开启以来总的滴答数
static void intr_timer_hanldler(void)
{
    struct task_struct * cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == STACK_MAGIC);

    cur_thread->elased_ticks++; //记录滴答数
    ticks++;//从第一次时钟终端开始到现在经历的总滴答数

    if(cur_thread->ticks == 0)  schedule();
    else    cur_thread->ticks--; 

}

// 初始化PIT8253
void timer_init()
{
    put_str("timer_init start\n");
    frequency_set(COUNTER0_PORT,COUNTER0_NO,READ_WRITE_LATCH,COUNTER_MODE,COUNTER0_VALUE);
    register_handler(0x20,intr_timer_hanldler);
    put_str("timer_init done\n");
}

