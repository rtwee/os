#ifndef __DEVICE_IOQUEUE_H__
#define __DEVICE_IOQUEUE_H__

#include "../lib/stdint.h"
#include "../thread/thread.h"
#include "../thread/sync.h"

#define bufsize 64
//环形队列
struct ioqueue
{
    //生产者消费者问题
    struct lock lock;
    //生产者，缓冲区未满就生产，满了就在这里休息
    struct task_struct * producer;

    //消费者，缓冲区有数据就拿，无数据就在此休息
    struct task_struct * consumer;
    char buf[bufsize];              //缓冲区
    int32_t head;                   //队首
    int32_t tail;                   //队尾
};


void ioqueue_init(struct ioqueue * ioq);
char ioq_getchar(struct ioqueue * ioq);
void ioq_putchar(struct ioqueue * ioq,char byte);
bool ioq_full(struct ioqueue * ioq);
 bool ioq_empty(struct ioqueue * ioq);

#endif