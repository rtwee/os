#ifndef __THREAD_SYNC_H__
#define __THREAD_SYNC_H__
#include "list.h"
#include "stdint.h"
#include "thread.h"

//信号量结构
struct semaphore
{
    uint8_t value;          //信号量的初值 down操作时它为0那就要阻塞线程
    struct list waiters;    //用来记录等待此信号量的所有线程.等待队列
};


//锁结构
struct lock
{
    struct task_struct * holder;    //锁的持有者
    struct semaphore semaphore;     //信号量，里面有等待锁的队列
    uint32_t holder_repeat_nr;      //持有者重复申请锁的次数,避免重复释放
};

void lock_init(struct lock * plock);
void lock_acquire(struct lock * plock);
void lock_release(struct lock * plock);
void sema_init(struct semaphore * psema,uint8_t value);
void sema_down(struct semaphore * psema);
void sema_up(struct semaphore * psema);
#endif