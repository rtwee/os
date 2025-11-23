%include "boot.inc"
SECTION loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
    jmp loader_start

GDT_BASE:
    dd 0x00000000
    dd 0x00000000
CODE_DESC:
    dd 0x0000FFFF
    dd DESC_CODE_HIGH4
DATA_STACK_DESC:
    dd 0x0000FFFF
    dd DESC_DATA_HIGH4
VIDEO_DESC:
    dd 0x80000007
    dd DESC_VIDEO_HIGH4

    GDT_SIZE  equ $ - GDT_BASE
    GDT_LIMIT equ GDT_SIZE - 1
;这个对齐数要算一下，书又错了 512
    times 59 dq 0
    times 5 db 0

; ;偏移地址是0x200 + 0x900 = 0xb00
    total_mem_bytes dd 0
; ;选择子一代表的是代码段
    SELECTOR_CODE  equ (0x0001<<3) + TI_GDT + RPL0
    SELECTOR_DATA  equ (0x0002<<3) + TI_GDT + RPL0
    SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0
; ;将来要加载的段描述表
    gdt_ptr dw GDT_LIMIT
            dd GDT_BASE

;-------------------这里对齐---------------------
    ards_buf times 244 db 0 
    ards_nr dw 0            ;对其256

loader_start:

    xor ebx,ebx         ;第一次调用时ebx要为0
    mov edx,0x534d4150  ;这是标签
    mov di,ards_buf     ;ards结构缓冲区
.e820_mem_get_loop:
    mov eax,0x0000e820  ;功能号,执行完0x15后，该值变成0x524d4150
    mov ecx,20          ;ARDS地址范围描述符的大小是20字节
    int 0x15
    jc .e820_failed_so_try_e801 ;如果CF位置1的话说明出错了

    add di,cx           ;使di增加20字节 指向缓冲区中的新的ARDS结构位置
    inc word [ards_nr]  ;记录结构体的数量
    cmp ebx,0           ;看一下是不是最后一个

    jnz .e820_mem_get_loop
    ;找出所有结构种base+length的最大值
    mov cx,[ards_nr]
    mov ebx,ards_buf
    xor edx,edx         ;edx为最大的内存容量，在此先清0
.find_max_mem_area:
    mov eax,[ebx]       ;base
    add eax,[ebx + 8]   ;length的最大值
    add ebx,20
    cmp edx,eax
    jge .next_ards
    mov edx,eax         ;edx为总内存大小
.next_ards:
    loop .find_max_mem_area
    jmp .mem_get_ok

.e820_failed_so_try_e801:
    mov ax,0xe801
    int 0x15
    jc .e801_failed_so_try88

;1 先算出低15MB内存
    mov cx,0x400
    mul cx
    shl edx,16
    and eax,0x0000FFFF
    or edx,eax
    add edx,0x100000
    mov esi,edx
;2 先将16MB以上的内存转化为byte单位
    xor eax,eax
    mov ax,bx
    mov ecx,0x10000
    mul ecx

    add esi,eax
    mov edx,esi
    jmp .mem_get_ok
.e801_failed_so_try88:
    mov ah,0x88
    int 0x15
    jc .error_hlt
    and eax,0x0000FFFF
    mov cx,0x400
    mul cx
    shl edx,16
    or edx,eax
    add edx,0x100000
.error_hlt:
    jmp $
.mem_get_ok:
    mov [total_mem_bytes],edx

;-----------------------打开A20
    in al,0x92
    or al,0000_0010B
    out 0x92,al

;-----------------------加载GDT
    lgdt [gdt_ptr]

;-----------------------cr0第0位置1
    mov eax,cr0
    or eax,0x00000001
    mov cr0,eax

    jmp dword SELECTOR_CODE:p_mode_start            ;刷新流水线


[bits 32]
p_mode_start:
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP

    mov eax,KERNEL_START_SECTOR
    mov ebx,KERNEL_BIN_BASE_ADDR
    mov ecx,200

    call rd_disk_m_32

    ;建立页表
    call setup_page
    sgdt [gdt_ptr]

    mov ebx,[gdt_ptr + 2]   ;base
    or dword [ebx + 0x18 + 4],0xc0000000
    add dword [gdt_ptr + 2],0xc0000000
    add esp,0xc0000000  

    ; ;页目录地址付给cr3
    mov eax,PAGE_DIR_TABLE_POS
    mov cr3,eax

    ; ;打开cr0的pg位
    mov eax,cr0
    or eax,0x80000000
    mov cr0,eax
    lgdt [gdt_ptr]

    mov eax,SELECTOR_VIDEO
    mov gs,eax

    mov byte [gs:160],'P'

    jmp SELECTOR_CODE:enter_kernel

;---------------------- 跳转到内核 ----------
;---------------------- 目前暂存的位置范围 0x70000 - 0x9f000
enter_kernel:
    call kernel_init    ;解析内核
    mov esp,0xc009f000  ;更新栈顶
    jmp KERNEL_ENTER_ADDR

;------------- 创建页目录及页表 -----------
setup_page:
    mov ecx,4096
    mov esi,0
.clear_page_dir:    ;把页目录表清空
    mov byte [PAGE_DIR_TABLE_POS + esi],0
    inc esi
    loop .clear_page_dir
.create_pde:        ;创建页目录项
    mov eax,PAGE_DIR_TABLE_POS
    add eax,0x1000  ;此时eax为第一个页表的位置  0x10000 + 0x1000
    mov ebx,eax     
    ;因为他的地址的低12位一定是0
    or eax, PG_US_U | PG_RW_W | PG_P
    mov [PAGE_DIR_TABLE_POS + 0x0],eax  
    mov [PAGE_DIR_TABLE_POS + 0xc00],eax    ;0xc00处的页目录项
    sub eax,0x1000
    mov [PAGE_DIR_TABLE_POS + 4092],eax     ;这是让最后一个页目录项指向了自己
    ;下面创建页表项(PTE)
    mov ecx,256 ;低端内存1M/4k=256
    mov esi,0
    mov edx,PG_US_U | PG_RW_W | PG_P
.create_pte:    ;;第一个页目录项,指向的页表是内核的地址
    mov [ebx+esi*4],edx
    add edx,4096
    inc esi
    loop .create_pte
    ;创建内核其他的PDE
    mov eax,PAGE_DIR_TABLE_POS
    add eax,0x2000
    or eax, PG_US_U | PG_RW_W | PG_P
    mov ebx,PAGE_DIR_TABLE_POS
    mov ecx,254
    mov esi,769 ;内核从769开始到1022
.create_kernel_pde:
    mov [ebx+esi*4],eax
    inc esi
    add eax,0x1000
    loop .create_kernel_pde
    ret

; ------------------- 把0x70000处的信息解析 放到 0xc0001500处
kernel_init:
    xor eax,eax
    xor ebx,ebx
    xor ecx,ecx
    xor edx,edx

    mov dx,[KERNEL_BIN_BASE_ADDR + 42]  ;e_phentsize表示程序头大小
    mov ebx,[KERNEL_BIN_BASE_ADDR + 28] ;e_phoff 表示第一个程序表头在文件中的偏移量

    add ebx,KERNEL_BIN_BASE_ADDR        ;ebx放的第一个有效的段头表地址
    mov cx,[KERNEL_BIN_BASE_ADDR + 44]  ;cx中存放了程序表头的个数

.each_segment:
    cmp byte [ebx+0],PT_NULL    ;段头表第一个元素是否为空
    je .PTNULL
    ;准备参数 从右往左入栈  memcpy (dst src size)
    push dword [ebx + 16]   ;大小参数
    mov eax,[ebx + 4]       ;p_offset 
    add eax,KERNEL_BIN_BASE_ADDR
    push eax                ;源
    push dword [ebx + 8]    ;p_vaddr
    call mem_cpy
    add esp,12              ;清理三个参数
.PTNULL:
    add ebx,edx             ;指向下一个

    loop .each_segment
    ret
;memcpy (dst src size)
mem_cpy:
    cld
    push ebp
    mov ebp,esp
    push ecx

    mov edi,[ebp + 8]
    mov esi,[ebp + 12]
    mov ecx,[ebp + 16]
    rep movsb

    pop ecx
    pop ebp
    ret

;--------------------- 把内核代码读取到0x70000处 包含了elf头信息 ---------
rd_disk_m_32:
;---------------------功能读取硬盘的cx个扇区--------------------------
;1.选择通道，往通道的sector count寄存器中写入待操作的扇区数量 ***对应第一步骤中的0x1f2给主盘的count寄存器传入尧都区的扇区数为cl
;2.往该通道的三个LBA寄存器写入扇区起始位置低24位
    mov esi,eax                     ;eax=LBA扇区号,0x2
    mov di,cx                       ;写入的山区数量位cx，在di中备份
;第一步设置要读取的扇区数量 主盘0x1f2
    mov dx,0x1f2
    mov al,cl                       
    out dx,al                       ;要读取的数量
    mov eax,esi                     ;恢复ax
;第二步写入LBA地址  0x1f3-0x1f5为低到高的24位
    mov dx,0x1f3
    out dx,al                       ;将2写道LBA的低地址

    mov cl,8
    shr eax,cl                      ;右边移动8位
    mov dx,0x1f4
    out dx,al

    shr eax,cl
    mov dx,0x1f5
    out dx,al

    shr eax,cl
    and al,0x0f                     ;25-18位是在0x1f6中的下半部分这是device寄存器
    or al,0xe0                      ;将4-7位置进行设置1110,表示lba模式
    mov dx,0x1f6
    out dx,al
;第三步 向0x1f7端口写入读命令,0x20
    mov dx,0x1f7
    mov al,0x20
    out dx,al
;第四步：检测硬盘状态
.not_ready:
    nop
    in al,dx
    and al,0x88                      ;第七位是BUSY位置，第四位是有效位，成功后设置为1
    cmp al,0x08
    jnz .not_ready

;第五步：从0x1f0读取数据
    mov ax,di                        ;di表示要写入的扇区数量
    mov dx,256
    mul dx
    mov cx,ax                        ;一个扇区又512个字节，每次读入16位，因此读入 256 * count次

    mov dx,0x1f0

.go_on_read:
    in ax,dx
    mov [ebx],ax                      ;bx放的是指定的位置，这个位置是内存中空的区域
    add ebx,2
    loop .go_on_read
    ret