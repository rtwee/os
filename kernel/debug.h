#ifndef __KERNEL_DEBUG_H__
#define __KERNEL_DEBUG_H__
void panic_spin(char * filename,int line,const char * func,const char * condition);

//__VA_ARGS__是预处理器所支持的专用符 ...表示可变参数
#define PANIC(...) panic_spin(__FILE__,__LINE__,__func__,__VA_ARGS__)

#ifdef NDEBUG
    #define ASSERT(CONDITION) ((void)0)
#else
    #define ASSERT(CONDITION)  \ 
        if((CONDITION)){}else {  \
            PANIC(#CONDITION); \     
        }
        //这个是将CONDITION转化位字符串
#endif

#endif