
#include "../lib/kernel/print.h"
#include "init.h"
#include "debug.h"
#include "../lib/string.h"
#include "memory.h"
#include "../thread/thread.h"
#include "interrupt.h"
#include "../device/console.h"
#include "../device/ioqueue.h"
#include "../device/keyboard.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../device/timer.h"
#include "../lib/stdio.h"
#include "../device/ide.h"
#include "../fs/fs.h"

void test_thread1(void* arg);
void test_thread2(void* arg);

extern struct ide_channel channels[2];         //有两个硬盘通道

int main(void) {
   put_str("I am kernel\n");
   init_all();
   thread_start("kernel_thread_a",31,test_thread1," A_");
   thread_start("kernel_thread_b",31,test_thread2," B_");
   intr_enable();

   char write_buf[2048]="zhe shi wo de di yi ge ce shi qing ni kan kan dui bu dui";
   char read_buf[2048]={0};

//    ide_write(&channels[0].devices[1],100,write_buf,2);
//    ide_read(&channels[0].devices[1],100,read_buf,2);

//    printk("context : %s",read_buf);

// //    struct ide_channel channel = channels[0];
//    struct disk hd = channels[0].devices[1];
//    partition_format(&hd,&hd.logic_parts[0]);
   while(1);
   return 0;
}

void test_thread1(void* arg)
{
    while(1)
    {
        enum intr_status old_status = intr_disable();
        while(!ioq_empty(&ioqueue))
        {
			
			// console_put_str((char*)arg);
			// char chr = ioq_getchar(&ioqueue);
			// console_put_char(chr);
            printk("%s_%c ",arg,ioq_getchar(&ioqueue));
		}

   		intr_set_intr(old_status);
    }
}

void test_thread2(void* arg)
{
    while(1)
    {
        enum intr_status old_status = intr_disable();
        while(!ioq_empty(&ioqueue))
        {
   	    // console_put_str((char*)arg);
    	//     char chr = ioq_getchar(&ioqueue);
   	    // console_put_char(chr);
            printk("%s_%c ",arg,ioq_getchar(&ioqueue));
	    }
   	intr_set_intr(old_status);
    }
}
