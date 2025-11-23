#ifndef __USERPROG_TSS_H__
#define __USERPROG_TSS_H__
#include "../thread/thread.h"
void tss_init();
void update_tss_esp(struct task_struct * pthread);

#endif