#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "user/syscall.h"
#include "../kernel/global.h"


//将整型转化成字符 int to ascii(buf算出来是反向的)
static void itoa(uint32_t value,char ** buf_ptr_addr,uint8_t base)
{
    uint32_t m = value % base;  //最低位
    uint32_t i = value / base;  //取高位
    if(i)   // 如果还有高位
    {
        itoa(i,buf_ptr_addr,base);
    }
    if(m < 10)  // 如果余数是0-9
    {
        *((*buf_ptr_addr)++) = m + '0';
    }
    else
    {
        *((*buf_ptr_addr)++) = m - 10 + 'A';
    }
}

//将参数ap按照格式format输出到字符str,并返回替换后的str长度
uint32_t vsprintf(char * str,const char * format,va_list ap)
{
    char * buf_ptr = str;
    char * arg_str;
    const char * index_ptr = format;
    char index_char = *index_ptr;
    int arg_int;
    while (index_char)  //若非0
    {
        if(index_char != '%')
        {
            *(buf_ptr++)=index_char;
            index_char = *(++index_ptr);
            continue;
        }
        index_char = *(++index_ptr);
        switch (index_char)
        {
            case 's':
                arg_str = va_arg(ap,char *);
                strcpy(buf_ptr,arg_str);
                buf_ptr += strlen(arg_str);
                index_char = *(++index_ptr);
                break;
            case 'c':
                *(buf_ptr++) = va_arg(ap,char);
                index_char = *(++index_ptr);
                break;
            case 'd':
                arg_int = va_arg(ap,int);
                if(arg_int < 0)
                {
                    arg_int = 0 - arg_int;
                    *buf_ptr++ = '-';
                }
                
                itoa(arg_int,&buf_ptr,10);
                index_char = *(++index_ptr);
                break;
        
            case 'x':
                arg_int = va_arg(ap,int);   //返回下一个指向的int数值
                itoa(arg_int,&buf_ptr,16); // 32位，最多是4位，也就是把32位的值改成了4个char
                index_char = *(++index_ptr);
            // 跳过字符串格式并且更新index_char;
            break;
        }
    }
    return strlen(str);
    
}

// 格式化输出format
uint32_t printf(const char * format,...)
{
    va_list args;
    va_start(args,format);
    char buf[1024] = {0};
    vsprintf(buf,format,args);
    va_end(args);
    return write(buf);
}

// 格式化输出format
uint32_t sprintf(char * str,const char * format,...)
{
    va_list args;
    va_start(args,format);
    vsprintf(str,format,args);
    va_end(args);
    return strlen(str);
}
