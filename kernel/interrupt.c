#include "interrupt.h"
#include "global.h"
#include "../lib/stdint.h"
#include "../lib/kernel/io.h"
#include "print.h"

#define IDT_DESC_CNT 0x81   // 中断的数量，一共有33个，我们要用第33个 + f == (0x21 + 0xf),现在又需要0x80来做中断了

//端口设置
#define PIC_M_CTRL 0x20     //主片的端口
#define PIC_M_DATA 0x21

#define PIC_S_CTRL 0xa0     //从片的端口
#define PIC_S_DATA 0xa1

#define EFLAGS_IF 0x00000200    //
#define GET_EFLAGS(FLAGS_VAR) asm volatile("pushfl;popl %0":"=g"(FLAGS_VAR))    //g表示任意寄存器或者内存

//这是将来构造的系统调用
extern uint32_t syscall_handler(void);


//中断描述符结构
struct gate_desc
{
    uint16_t func_offset_low_word;  //偏移地址 低16位
    uint16_t selector;              //目标代码段所在的 代码段选择子
    uint8_t dcount;                 //中断 中断这里是空
    uint8_t attribute;              //设置DPL TYPE啥的属性
    uint16_t func_offset_high_word; //偏移地址 高16位
};

//中断描述符表  将来加载到IDT中
static struct gate_desc idt[IDT_DESC_CNT];

//用于保存异常的名字
char * intr_name[IDT_DESC_CNT];

//将来汇编调用的中断处理函数
intr_handler idt_table[IDT_DESC_CNT];

//这是再kernel.S 中给每个中断构造的入口函数
extern intr_handler intr_entry_table[IDT_DESC_CNT];

//通用的中断处理函数，一般在异常的时候出现  这里输入的是中断号
static void general_intr_handler(uint8_t vec_nr)
{
    if(vec_nr == 0x27 || vec_nr == 0x2f)
    {
        //IRQ7 和 IRQ15 会产生伪中断 不用处理
        return;
    }
    set_cursor(0);
    int cursor_pos = 0;
    while(cursor_pos < 320)
    {
        put_char(' ');
        cursor_pos++;
    }
    set_cursor(0);
    put_str("!!!!!!! excetion message begin !!!!!!\n");
    set_cursor(88);
    put_str("intr_name: ");
    put_str(intr_name[vec_nr]);put_char('\n');
    if(vec_nr == 14)
    {
        int page_fault_addr = 0;
        asm("movl %%cr2,%0":"=r"(page_fault_addr));
        put_str("\n page fault addr is ");put_int(page_fault_addr);
    }
    put_str("!!!!!!! excetion message end  !!!!!!\n");
    while(1);
}

// 完成一般中断处理函数注册 及 异常名称的注册
static void exception_init(void)
{
    int i;
    for(i=0;i < IDT_DESC_CNT;++i)
    {
        idt_table[i]=general_intr_handler;
        intr_name[i]="unknow";
    }
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[15] 第15项是intel保留项，未使用
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

//初始化可编程中断控制器8259A
static void pic_init()
{
    //初始化主片
                            //设置级联 边沿触发 的x86
    outb(PIC_M_CTRL,0x11);  //高位0001固定  低位0：边沿触发   0:ADI不需要设置   0:级联 1:x86要设置为1 
    outb(PIC_M_DATA,0x20);  //设置起始中断向量号，我们的IRQ0在第33个取值32
    outb(PIC_M_DATA,0x04);  //主片的IRQ2 上链接了个从片
    outb(PIC_M_DATA,0x01);  //非自动EOI 且 为x86
    //初始化从片
    outb(PIC_S_CTRL,0x11);  //高位0001固定  低位0：边沿触发   0:ADI不需要设置   0:级联 1:x86要设置为1 
    outb(PIC_S_DATA,0x28);  //设置起始中断向量号，从片的向量起始是0x28 可以看图7-11
    outb(PIC_S_DATA,0x02);  //链接到了主片的IRQ2 低三位设为010
    outb(PIC_S_DATA,0x01);  //非自动EOI 且 为x86

    //打开主片上的IRQ0,也就是目前只接受时钟产生的中断
    outb(PIC_M_DATA,0xf8);  //打开时钟 和 键盘中断 磁盘中断
    outb(PIC_S_DATA,0xbf);  //从片磁盘中断

    put_str("pic_init done\n");
}


//构造 中断描述符
static void make_idt_desc(struct gate_desc * p_gdesc,uint8_t attr,intr_handler function)
{
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount=0;
    p_gdesc->attribute=attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

//初始化中断描述符表
static void idt_desc_init()
{
    int i,lastindex = IDT_DESC_CNT - 1;
    for(int i = 0;i < IDT_DESC_CNT;++i)
        make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0,intr_entry_table[i]);
    //系统调用的中断DPL对应DPL3 == 否则在DPL为0，3环境下执行int会发生GP异常
    // int 0x80发生中断后调用syscall——handler，由于eax中包含系统调用的编号，因此可以执行系统调用
    make_idt_desc(&idt[lastindex],IDT_DESC_ATTR_DPL3,syscall_handler);
    put_str(" idt_desc_init done\n");
}

//完成有关中断的所有初始化工作
void idt_init()
{
    put_str("idt_init start\n");
    idt_desc_init();
    exception_init();
    pic_init();

    //idt往idtr加载时的数据格式  高32 中断描述符表基址 | 低16位 表的大小
    uint64_t idt_operand = ((sizeof(idt) -1) |  ((uint64_t) (uint32_t)idt << 16));
    asm volatile("lidt %0"::"m"(idt_operand));
    put_str("idt_init_done\n");
}

//获取当前中断状态
enum intr_status intr_get_intr()
{
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}


//开中断 并返回中断前的状态
enum intr_status intr_enable()
{
    enum intr_status old_status;
    if(INTR_ON == intr_get_intr())
    {
        old_status = INTR_ON;
        return old_status;
    }
    else
    {
        old_status = INTR_OFF;
        asm volatile("sti");
        return old_status;
    }
}

//关中断 并返回中断前的状态
enum intr_status intr_disable()
{
    enum intr_status old_status;
    if(INTR_ON == intr_get_intr())
    {
        old_status = INTR_ON;
        asm volatile("cli");
        return old_status;
    }
    else
    {
        old_status = INTR_OFF;
        return old_status;
    }
}
enum intr_status intr_get_status()
{
    return intr_get_intr();
}
//将当前的中断设置为status
enum intr_status intr_set_intr(enum intr_status status)
{
    return status & INTR_ON ? intr_enable():intr_disable();
}

//注册中断函数
void register_handler(uint8_t vector_no,intr_handler function)
{
    idt_table[vector_no] = function;
}