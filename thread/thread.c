#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "list.h"
#include "../kernel/memory.h"
#include "interrupt.h"
#include "../userprog/process.h"
#include "sync.h"

struct task_struct * idle_thread;   //idle线程
struct task_struct * main_trhead;   //主线程的PCB
struct list thread_ready_list;      //就绪队列
struct list thread_all_list;        //所有任务队列
static struct list_elem * thread_tag;//用于保存队列中的线程结点

struct lock pid_lock;               //用于设置pid的锁


//用于任务切换
extern void switch_to(struct task_struct * cur,struct task_struct * next);

//系统空闲时运行的线程


//获取当前线程的pcb指针
struct task_struct * running_thread()
{
    uint32_t esp;
    asm("mov %%esp,%0":"=g"(esp));
    return (struct task_struct *)(esp & 0xfffff000);//因为是按页分配的
}

static pid_t allocate_pid(void)
{
    static pid_t next_pid = 0;
    lock_acquire(&pid_lock);
    next_pid++;
    lock_release(&pid_lock);
    return next_pid;
}

static void kernel_thread(thread_func * function,void * func_arg)
{
    //避免后面的时钟中断被屏蔽，没办法调度了  因为是在中断中跳转过来的
    intr_enable();
    function(func_arg);
}

//初始化线程栈
void thread_create(struct task_struct * pthread,thread_func function,void * func_arg)
{
    pthread->self_stack -= sizeof(struct intr_stack);
    pthread->self_stack -= sizeof(struct thread_stack); //PCB中拿到 内核栈，也就是栈顶  预留线程栈
    struct thread_stack * kthread_stack = (struct thread_stack*)pthread->self_stack;//这是从PCB中拿到 内核栈,内核栈的开始部分作为thread_stack
    kthread_stack->eip = kernel_thread; //高地址到低地址
                                        //这里起始有个used地址
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

//初始化线程基本信息
void init_thread(struct task_struct * pthread,char * name,int prio)
{
    memset(pthread,0,sizeof(*pthread));
    pthread->pid = allocate_pid();
    strcpy(pthread->name,name);

    if(pthread == main_trhead)  pthread->status = TASK_RUNNING;//初始化主线程的时候，主线程本身就已经运行了 bro
    else pthread->status = TASK_READY;
    pthread->self_stack=(uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority=prio;
    pthread->ticks=prio;
    pthread->elased_ticks=0;
    pthread->pgdir=NULL;
    pthread->stack_magic = STACK_MAGIC; //PCB的边界

}

//创建一个线程
struct task_struct * thread_start(char * name,int prio,thread_func function,void * func_arg)
{
    //申请了 内核的页
    struct task_struct * thread = get_kernel_pages(1);
    //初始化PCB
    init_thread(thread,name,prio);
    //构建 能够使用return 返回到function的栈
    thread_create(thread,function,func_arg);

    //确保之前不在队列中
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);

    //确保之前不在队列中
    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag); 
    //现在的线程创建 只是用到了队列 加入队列后等待调度即可
    return thread;
}


//创建主线程
static void make_main_thread(void)
{
    main_trhead = running_thread();//0xc009e000
    init_thread(main_trhead,"main",31);
    //确保之前不在队列中 当前线程 在运行 只需要在总的里面就行
    ASSERT(!elem_find(&thread_all_list,&main_trhead->general_tag));
    list_append(&thread_all_list,&main_trhead->all_list_tag); 
}

//调度
void schedule()
{
    //是从中断调用过来的  此时是关中断的状态
    ASSERT(intr_get_status() == INTR_OFF);

    struct task_struct * cur = running_thread();
    if(cur->status == TASK_RUNNING)
    {
        ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));
        list_append(&thread_ready_list,&cur->general_tag);
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    }
    else
    {
        //若是非就绪状态那算了
    }

    // ASSERT(!list_empty(&thread_ready_list));
    if(list_empty(&thread_ready_list))
    {
        //为空的时候解放idle，那这个时候就可以执行idle，这时回回到hlt命令
        thread_unblock(idle_thread);
    }
    thread_tag = NULL;

    thread_tag = list_pop(&thread_ready_list);
    struct task_struct * next = elem2entry(struct task_struct,general_tag,thread_tag);
    next->status = TASK_RUNNING;
    //激活任务页表等
    process_activate(next);
    
    switch_to(cur,next);
}

void thread_init(void)
{
    put_str("thread init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    lock_init(&pid_lock);
    make_main_thread();
    //创建idle线程 //没有任务进行的时候来执行idle
    
    put_str("thread inin done\n");
}

//当前线程将自己阻塞，标志旗状态位stat
void thread_block(enum task_status stat)
{
    //也就是要设置自己的阻塞状态
    ASSERT((stat == TASK_BLOCKED) || (stat == TASK_WAITING)|| (stat == TASK_HANGING));
    enum intr_status old_status = intr_disable();
    struct task_struct * cur_thread = running_thread();
    cur_thread->status = stat;
    schedule();//设置自己为阻塞状态 然后调用switch直接切换任务  这里的下面部分只能等待之后的重新调度才能执行下去
    intr_enable(old_status);
}

//将线phhread解除阻塞
void thread_unblock(struct task_struct * pthread)
{
    enum intr_status old_status = intr_disable();
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_HANGING) ||(pthread->status == TASK_WAITING)));
    if(pthread->status != TASK_READY)
    {
        ASSERT(!elem_find(&thread_ready_list,&pthread->general_tag));
        if(elem_find(&thread_ready_list,&pthread->general_tag))
        {
            PANIC("thread_unblock: block thread in ready_list\n");
        }
        list_push(&thread_ready_list,&pthread->general_tag);
        pthread->status = TASK_READY;
    }

    intr_set_intr(old_status);
}

//主动让出cpu
void thread_yield(void)
{
    struct task_struct * cur = running_thread();
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));
    list_append(&thread_ready_list,&cur->general_tag);
    cur->status=TASK_READY;
    schedule();
    intr_set_intr(old_status);
}

