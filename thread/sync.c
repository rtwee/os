#include "sync.h"
#include "interrupt.h"
#include "debug.h"

//初始化信号量
void sema_init(struct semaphore * psema,uint8_t value)
{
    psema->value = value;
    list_init(&psema->waiters);
}

//初始化锁
void lock_init(struct lock * plock)
{
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore,1); //锁的话 信号量 0 或者 1
}

//信号量的down操作
void sema_down(struct semaphore * psema)
{
    enum intr_status old_status = intr_disable();
    while(psema->value == 0)    //表示已经被别人持有了 // 这里用while是为了避免多个线程竞争，苏醒后不一定能够拿到锁还是得继续阻塞
    {
        ASSERT(!elem_find(&psema->waiters,&running_thread()->general_tag));//并且当前线程不再等待队列中
        if(elem_find(&psema->waiters,&running_thread()->general_tag))
        {
            PANIC("sema donw: thread blocked has been in waiters_list\n");
        }
        list_append(&psema->waiters,&running_thread()->general_tag);
        thread_block(TASK_BLOCKED);
    }
    psema->value--;
    ASSERT(psema->value == 0);
    intr_set_intr(old_status);
}

//信号量的up操作
void sema_up(struct semaphore * psema)
{
    enum intr_status old_status = intr_disable();
    ASSERT(psema->value == 0);
    if(!list_empty(&psema->waiters))
    {
        struct task_struct * thread_blocked = elem2entry(struct task_struct,general_tag,list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }
    psema->value++;
    ASSERT(psema->value == 1);
    intr_set_intr(old_status);
}

// 获取锁plock
void lock_acquire(struct lock * plock)
{
    if(plock->holder != running_thread())
    {
        //申请锁拿不到就阻塞自己等待调度
        sema_down(&plock->semaphore);
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);   ////第一次申请锁，申请结束能拿到的资源一定是1
        plock->holder_repeat_nr = 1;
    }
    else
    {
        plock->holder_repeat_nr++;
    }
}

//释放锁
void lock_release(struct lock * plock)
{
    ASSERT(plock->holder == running_thread());
    if(plock->holder_repeat_nr > 1)
    {
        plock->holder_repeat_nr--;
        return;
    }
    ASSERT(plock->holder_repeat_nr == 1);

    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    //其他线程没有锁就打断不了他的这个资源
    sema_up(&plock->semaphore);
}