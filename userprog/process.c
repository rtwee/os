#include "process.h"
#include "debug.h"
#include "../thread/thread.h"
#include "../kernel/memory.h"
#include "tss.h"
#include "userprog.h"
#include "../device/console.h"
#include "../lib/string.h"
#include "../kernel/interrupt.h"
#include "../lib/kernel/list.h"

extern void intr_exit(void);
extern struct list thread_ready_list; 
extern struct list thread_all_list;

//构建用户进程初始上下文信息 //这是在任务页表创建后才执行的
void start_process(void * filename_)
{
    void * function = filename_;
    struct task_struct * cur = running_thread();
    // cur->self_stack += sizeof(struct task_struct); //跳过线程栈 直接来设置中断栈
    struct intr_stack * proc_stack = (struct intr_stack *)cur->self_stack + sizeof(struct task_struct);
    proc_stack->edi = proc_stack->esp_dummy = proc_stack->ebp = proc_stack->esi = 0;

    proc_stack->ebx = proc_stack->edx=proc_stack->ecx=proc_stack->eax = 0;
    proc_stack->gs = 0; //用户态是没办法用的
    proc_stack->ds = proc_stack->es=proc_stack->fs = SELECTOR_U_DATA;//特权级为3的用户选择子
    proc_stack->eip = function;
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFKAGS_IOPL_0 | EFLAGS_MBS|EFLAGS_IF_1);
    proc_stack->esp = (void *)((uint32_t)get_a_page(PF_USER,USER_STACK3_VADDR) + PG_SIZE);//3特权级的栈顶
    proc_stack->ss = SELECTOR_U_DATA;
    asm volatile ("movl %0,%%esp;jmp intr_exit"::"g"(proc_stack):"memory");
}

//激活页表
void page_dir_activate(struct task_struct * p_thread)
{
    uint32_t pagedir_phy_addr = 0x100000; //内核线程重新填充页表
    if(p_thread->pgdir != NULL){
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
    }
    //更新页目录寄存器cr3
    asm volatile("mov %0,%%cr3"::"r"(pagedir_phy_addr):"memory");
}


//激活线程或者进程的页表，更新tss中的esp0为进程的特权级0的栈
void process_activate(struct task_struct * p_thread)
{
    ASSERT(p_thread != NULL);
    //激活该进程或者线程的页表
    page_dir_activate(p_thread);

    //内核线程特权级本身就是0,处理器进入中断时并不会从tss中获取0特权级栈地址，
    //故不需要更新esp0
    if(p_thread->pgdir)
    {
        update_tss_esp(p_thread);
    }

}

//创建页目录表，将当前页表的表示内核空间的pde复制,成功则返回页目录表的虚拟地址,否则返回-1
uint32_t create_page_dir(void)
{
    uint32_t * page_dir_vaddr = get_kernel_pages(1);//进程的页表不能让用户访问到，因此申请内核的空间
    if(page_dir_vaddr == NULL)
    {
        console_put_str("create_page_dir:get_kernel_page failed!");
        return NULL;
    }

    //1.先复制页表,内核从768项==0x300
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4),(uint32_t*)(0xfffff000+0x300*4),1024);//256*4=1024

    //2.更新页目录地址
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);

    page_dir_vaddr[1023]=(new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1);//页目录最后一项指向自己

    return page_dir_vaddr;
}

//创建用户进程虚拟地址位图
void create_user_addr_bitmap(struct task_struct * user_prog)
{
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    //计算存放位图需要的页表数,在内核中申请页表存放位图
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START)/PG_SIZE/8,PG_SIZE);
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len=(0xc0000000 - USER_VADDR_START)/PG_SIZE/8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);

}

//创建用户进程
void process_execute(void * filename,char * name)
{
    struct task_struct * thread = get_kernel_pages(1);
    init_thread(thread,name,default_prio);
    create_user_addr_bitmap(thread);
    thread_create(thread,start_process,filename);
    thread->pgdir = create_page_dir();
    block_desc_init(thread->u_block_desc);  //用户的arena

    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    list_append(&thread_ready_list,&thread->general_tag);

    ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
    list_append(&thread_all_list,&thread->all_list_tag);
    intr_set_intr(old_status);
}