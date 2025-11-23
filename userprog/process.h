#ifndef __USERPROG_PROCESS_H__
#define __USERPROG_PROCESS_H__
#include "../thread/thread.h"

void start_process(void * filename_);
void process_activate(struct task_struct * p_thread);
void process_execute(void * filename,char * name);
#endif