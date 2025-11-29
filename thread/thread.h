#ifndef __THREAD_THREAD_H__
#define __THREAD_THREAD_H__

#include "../lib/stdint.h"
#include "list.h"
#include "../kernel/memory.h"

#define PG_SIZE 4096

typedef void thread_func(void*);
typedef uint32_t pid_t;

enum task_status
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

//中断栈 intr_stack 调用中断处理函数 中 注册函数前要保存的数据
struct intr_stack
{
    uint32_t vec_no;//中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;

    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    uint32_t error_code;
    void (*eip)(void);
    uint32_t cs;
    uint32_t eflags;
    void * esp;  //这个是有特权转变的时候能用到
    uint32_t ss;
};

//线程栈
struct thread_stack
{
//  ABI的规范 这几个寄存器加 esp都是由调用方维护的 
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
//首次执行线程的时候用eip来指示函数地址
    void (*eip)(thread_func * func,void * func_arg);
//当 任务切换时，eip用于保存任务切换后的新任务的返回地址
//下面这几个参数只在第一次被调用时使用,调用eip的时候模拟的栈
    void(*unused_retaddr);  //充当返回地址 占位的
    thread_func * function; //调用的函数名，就是线程中执行的函数
    void * func_arg;        //是线程所需要的参数
};

struct task_struct
{
    uint32_t * self_stack;
    pid_t pid;
    enum task_status status;
    char name[16];
    uint8_t priority;

    uint8_t ticks;          //每次在处理器上执行的时间滴答数
    uint32_t elased_ticks;  //总运行的滴答数
    

    //在一般 任务队列的标签
    struct list_elem general_tag;
    //在总任务队列中的标签
    struct list_elem all_list_tag;

    //进程自己页表的虚拟地址
    uint32_t * pgdir;
    //用户进程的虚拟地址池
    struct virtual_addr userprog_vaddr;
    struct mem_block_desc u_block_desc[DESC_CNT]; //用户进程的内存块描述符
    uint32_t stack_magic;
};

struct task_struct * thread_start(char * name,int prio,thread_func function,void * func_arg);
struct task_struct * running_thread();
void schedule();
void thread_init();
void thread_block(enum task_status stat);//阻塞自己
void thread_unblock(struct task_struct * pthread);//解救别人
void init_thread(struct task_struct * pthread,char * name,int prio);
void thread_create(struct task_struct * pthread,thread_func function,void * func_arg);
void thread_yield(void);
#endif