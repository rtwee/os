#include "init.h"
#include "../lib/kernel/print.h"
#include "interrupt.h"
#include "../device/timer.h"
#include "./memory.h"
#include "../thread/thread.h"
#include "../device/console.h"
#include "../userprog/tss.h"
#include "../userprog/syscall-init.h"
#include "../device/keyboard.h"
#include "../device/ide.h"

//负责所有模块的初始化
void init_all()
{
    put_str("init_all\n");
    idt_init();
    mem_init();//内存初始化
    timer_init(); //开启定时器
    keyboard_init();
    thread_init();
    console_init();
    tss_init();
    
    syscall_init();
    ide_init();
    
}