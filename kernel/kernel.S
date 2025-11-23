[bits 32]
%define ERROR_CODE nop  ;这是对于那种有错误码的
%define ZERO push 0     ;这是对于那种没有错误码的将来手动压入一个错误码

extern put_str          ;声明外部函数
extern idt_table
extern syscall_table


section .data 

global intr_entry_table
intr_entry_table:       ;数组 其中每个位置都是一个函数入口地址

%macro VECTOR 2         ;宏vector 有两个参数
section .text 
intr%1lentry:           ;给每个中断处理函数取个不同的名字
    
    %2                  ;将来这里会被ERROR_CODE 或者 ZERO 覆盖，来保证整个栈的一致性(无论有没有错误码,把他的格式统一)
    push ds             ;保护现场所有的段 和 寄存器  现场就保护了
    push es
    push fs
    push gs
    pushad

    mov al,0x20         ;给主片和从片手动的发送EOI命令
    out 0xa0,al
    out 0x20,al

    push %1
    call [idt_table + %1*4]

    jmp intr_exit

section .data
    dd intr%1lentry
%endmacro

section .text
global intr_exit
intr_exit:
    add esp,4
    popad
    pop gs 
    pop fs 
    pop es 
    pop ds 
    add esp,4   ;error_code
    iretd

;;;;;;;;;;;;;;;;;;;;;;;;; 0x80号中断 ;;;;;;;;;;;;;;;;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
;1.保存上下文环境
    push 0      ;压入0，使栈中格式统一

    push ds
    push es
    push fs
    push gs

    pushad      ;PUSHD压入32位寄存器，入栈顺序是
                ;EAX,ECX,EDX，EBX，ESP，EBP，ESI，EDI
    push 0x80   ;保持格式统一

;2.位系统调用子功能传入参数
    push edx    ;系统调用中第3个参数
    push ecx    ;系统调用中第2个参数
    push ebx    ;系统调用中第1个参数
;3.调用子功能处理函数---现在栈中也把参数传入了
    call [syscall_table + eax *4]
    add esp,12  ;参数弹栈
;4.将call调用后的返回值存入当前的内核栈中eax的位置
    mov [esp + 8*4],eax ;将调用后的返回值存到要返回的eax中
    jmp intr_exit



;------------------------------ 手动创建了33个基本的中断处理函数 --------------------------
;0-31 BIOS和系统给异常都分配了  32是我们自己给 IRQ0  也就是时钟中断分配的
VECTOR 0x0 ,ZERO
VECTOR 0X1 ,ZERO
VECTOR 0X2 ,ZERO
VECTOR 0x3 ,ZERO
VECTOR 0X4 ,ZERO
VECTOR 0X5 ,ZERO
VECTOR 0x6 ,ZERO
VECTOR 0X7 ,ZERO
VECTOR 0X8 ,ERROR_CODE
VECTOR 0x9 ,ZERO
VECTOR 0XA ,ERROR_CODE
VECTOR 0XB ,ERROR_CODE
VECTOR 0XC ,ERROR_CODE
VECTOR 0XD ,ERROR_CODE
VECTOR 0XE ,ERROR_CODE
VECTOR 0XF ,ZERO
VECTOR 0X10 ,ZERO
VECTOR 0X11 ,ERROR_CODE
VECTOR 0x12 ,ZERO
VECTOR 0X13 ,ZERO
VECTOR 0X14 ,ZERO
VECTOR 0x15 ,ZERO
VECTOR 0X16 ,ZERO
VECTOR 0X17 ,ZERO
VECTOR 0X18 ,ZERO
VECTOR 0X19 ,ZERO
VECTOR 0X1A ,ZERO
VECTOR 0X1B ,ZERO
VECTOR 0X1C ,ZERO
VECTOR 0X1D ,ZERO
VECTOR 0X1E ,ERROR_CODE                               ;处理器自动推错误码
VECTOR 0X1F ,ZERO
                    ;这里就是级联的哪个8259A 主片
VECTOR 0X20 ,ZERO   ;这是时钟中断
VECTOR 0X21 ,ZERO   ;这是键盘中断
VECTOR 0X22 ,ZERO   ;级联的从片
VECTOR 0X23 ,ZERO   ;串口2
VECTOR 0X24 ,ZERO   ;串口1
VECTOR 0X25 ,ZERO   ;并口2
VECTOR 0X26 ,ZERO   ;软盘
VECTOR 0X27 ,ZERO   ;并口1
                    ;                       从片
VECTOR 0X28 ,ZERO   ;实时时钟
VECTOR 0X29 ,ZERO   ;重定向的级联口
VECTOR 0X2a ,ZERO   ;保留
VECTOR 0X2b ,ZERO   ;保留
VECTOR 0X2c ,ZERO   ;PS/2鼠标
VECTOR 0X2d ,ZERO   ;FPU异常
VECTOR 0X2e ,ZERO   ;硬盘
VECTOR 0X2f ,ZERO   ;保留
VECTOR 0x30 ,ZERO
VECTOR 0x31 ,ZERO
VECTOR 0x32 ,ZERO
VECTOR 0x33 ,ZERO
VECTOR 0x34 ,ZERO
VECTOR 0x35 ,ZERO
VECTOR 0x36 ,ZERO
VECTOR 0x37 ,ZERO
VECTOR 0x38 ,ZERO
VECTOR 0x39 ,ZERO
VECTOR 0x3A ,ZERO
VECTOR 0x3B ,ZERO
VECTOR 0x3C ,ZERO
VECTOR 0x3D ,ZERO
VECTOR 0x3E ,ZERO
VECTOR 0x3F ,ZERO
VECTOR 0x40 ,ZERO
VECTOR 0x41 ,ZERO
VECTOR 0x42 ,ZERO
VECTOR 0x43 ,ZERO
VECTOR 0x44 ,ZERO
VECTOR 0x45 ,ZERO
VECTOR 0x46 ,ZERO
VECTOR 0x47 ,ZERO
VECTOR 0x48 ,ZERO
VECTOR 0x49 ,ZERO
VECTOR 0x4A ,ZERO
VECTOR 0x4B ,ZERO
VECTOR 0x4C ,ZERO
VECTOR 0x4D ,ZERO
VECTOR 0x4E ,ZERO
VECTOR 0x4F ,ZERO
VECTOR 0x50 ,ZERO
VECTOR 0x51 ,ZERO
VECTOR 0x52 ,ZERO
VECTOR 0x53 ,ZERO
VECTOR 0x54 ,ZERO
VECTOR 0x55 ,ZERO
VECTOR 0x56 ,ZERO
VECTOR 0x57 ,ZERO
VECTOR 0x58 ,ZERO
VECTOR 0x59 ,ZERO
VECTOR 0x5A ,ZERO
VECTOR 0x5B ,ZERO
VECTOR 0x5C ,ZERO
VECTOR 0x5D ,ZERO
VECTOR 0x5E ,ZERO
VECTOR 0x5F ,ZERO
VECTOR 0x61 ,ZERO
VECTOR 0x62 ,ZERO
VECTOR 0x63 ,ZERO
VECTOR 0x64 ,ZERO
VECTOR 0x65 ,ZERO
VECTOR 0x66 ,ZERO
VECTOR 0x67 ,ZERO
VECTOR 0x68 ,ZERO
VECTOR 0x69 ,ZERO
VECTOR 0x6A ,ZERO
VECTOR 0x6B ,ZERO
VECTOR 0x6C ,ZERO
VECTOR 0x6D ,ZERO
VECTOR 0x6E ,ZERO
VECTOR 0x6F ,ZERO
VECTOR 0x70 ,ZERO
VECTOR 0x71 ,ZERO
VECTOR 0x72 ,ZERO
VECTOR 0x73 ,ZERO
VECTOR 0x74 ,ZERO
VECTOR 0x75 ,ZERO
VECTOR 0x76 ,ZERO
VECTOR 0x77 ,ZERO
VECTOR 0x78 ,ZERO
VECTOR 0x79 ,ZERO
VECTOR 0x7A ,ZERO
VECTOR 0x7B ,ZERO
VECTOR 0x7C ,ZERO
VECTOR 0x7D ,ZERO
VECTOR 0x7E ,ZERO
VECTOR 0x7F ,ZERO
VECTOR 0x80 ,ZERO


