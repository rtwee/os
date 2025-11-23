#include "syscall.h"

//无参数的系统调用
//大括号宏定义，大括号中最后一个语句的值作为代码块的返回值,而且最后一句要加分号
#define _syacall0(NUMBER) ({ \
    int retval;              \
    asm volatile ("int $0x80":"=a"(retval):"a"(NUMBER):"memory"); \
    retval;                  \
})

#define _syacall1(NUMBER,ARG1) ({ \
    int retval;             \
    asm volatile ("int $0x80":"=a"(retval):"a"(NUMBER),"b"(ARG1):"memory"); \
    retval;                 \
})

#define _syacall2(NUMBER,ARG1,ARG2) ({ \
    int retval;             \
    asm volatile ("int $0x80":"=a"(retval):"a"(NUMBER),"b"(ARG1),"c"(ARG2):"memory"); \
    retval;                 \
})

#define _syacall3(NUMBER,ARG1,ARG2,ARG3) ({ \
    int retval;             \
    asm volatile ("int $0x80":"=a"(retval):"a"(NUMBER),"b"(ARG1),"c"(ARG2),"d"(ARG3):"memory"); \
    retval;                 \
})

//返回当前任务pid
uint32_t getpid()
{
    return _syacall0(SYS_GETPID);
}

//打印字符串
uint32_t write(char * str)
{
    return _syacall1(SYS_WRITE,str);
}

void * malloc(uint32_t size_t)
{
    return _syacall1(SYS_MALLOC,size_t);
}
void free(void * ptr)
{
    return _syacall1(SYS_FREE,ptr);
}