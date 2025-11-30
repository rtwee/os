#include "ide.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../kernel/debug.h"
#include "../lib/stdio.h"
#include "../lib/kernel/io.h"
#include "../device/timer.h"
#include "../kernel/interrupt.h"


#define reg_data(channel)       (channel->port_base + 0)
#define reg_error(channel)      (channel->port_base + 1)

#define reg_sect_cnt(channel)   (channel->port_base + 2)
#define reg_lba_l(channel)      (channel->port_base + 3)
#define reg_lba_m(channel)      (channel->port_base + 4)
#define reg_lba_h(channel)      (channel->port_base + 5)
#define reg_dev(channel)        (channel->port_base + 6)
#define reg_status(channel)     (channel->port_base + 7)
#define reg_cmd(channel)        (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel)        (reg_alt_status(channel))

// status 寄存器的一些位，我们要通过它判断是否可用/就绪
#define BIT_ALT_STAT_BSY        0x80    //硬盘忙
#define BIT_ALT_STAT_DRDY       0x40    //设备好了，等待命令
#define BIT_ALT_STAT_DRQ        0x8     //硬盘准备好数据，可以传输了

// device 寄存器的关键位
#define BIT_DEV_MBS             0xa0    //7和5固定为1
#define BIT_DEV_LBA             0x40    //LBA模式
#define BIT_DEV_DEV             0x10    //主盘还是从盘

// 一些硬盘操作指令
#define CMD_IDENTIFY            0xec    //identify指令
#define CMD_READ_SECTOR         0x20    //读
#define CMD_WRITE_SECTOR        0x30    //写

//定义可读写的最大扇区数
#define max_lba ((80 *1024*1024/512) -1)    //80M的lab数量

uint8_t channel_cnt;                    //按硬盘数量计算通道数
struct ide_channel channels[2];         //有两个硬盘通道


// 用于记录总扩展分区的起始lba，初始为0，partition_scan时以此为标记
int32_t ext_lba_base = 0;

uint8_t p_no = 0, l_no = 0;             //用来记录硬盘 主分区 和 逻辑分区 的下标

struct list partition_list;             //分区队列

// 构建一个16字节大小的结构体，用来存分区表项

struct partition_table_entry
{
    uint8_t bootable;   //是否可以引导
    uint8_t start_head; //起始磁头号
    uint8_t start_sec;  //起始扇区号
    uint8_t start_chs;  //起始柱面号
    uint8_t fs_type;    //分区类型
    uint8_t end_head;   //结束磁头号
    uint8_t end_sec;    //结束扇区号
    uint8_t end_chs;    //结束柱面号
    // *** important ***
    uint32_t start_lba; //本分区起始的扇区的lba地址
    uint32_t sec_cnt;   //本分区的扇区数目
} __attribute__ ((packed));


//引导扇区 mbr 或者 ebr 所在的扇区
struct boot_sector
{
    uint8_t other[446]; //引导代码
    struct partition_table_entry partition_table[4];    //分区表64字节
    uint16_t signature;                 //结束标志 0x55 0xaa
} __attribute__ ((packed));


// 选择读写的硬盘
static void slect_disk(struct disk * hd)
{
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    if(hd->dev_no == 1)
    {
        reg_device |= BIT_DEV_DEV;
    }
    // 设置dev寄存器
    outb(reg_dev(hd->my_channel),reg_device);
}

//设置操作的起始扇区 和 数量 扇区数(device都准备好了)
static void select_sector(struct disk * hd,uint32_t lba,uint8_t sec_cnt)
{
    ASSERT(lba <= max_lba);
    struct ide_channel * channel = hd->my_channel;
    // 写入要读写的扇区数
    outb(reg_sect_cnt(channel),sec_cnt);
    //写入lba地址
    outb(reg_lba_l(channel),lba);
    outb(reg_lba_m(channel),lba >> 8);
    outb(reg_lba_h(channel),lba >> 16);

    outb(reg_dev(channel),BIT_DEV_MBS|BIT_DEV_LBA | (hd->dev_no == 1?BIT_DEV_DEV:0)| lba >> 24);
}

// 向通道channel发命令cmd
static void cmd_out(struct ide_channel * channel,uint8_t cmd)
{
    //只要向硬盘发送了命令，我们都应该标记，
    channel->expecting_intr = true;
    outb(reg_cmd(channel),cmd);
}


// 硬盘读入sec_cnt个扇区的数据到buf
// 0: 256
static void read_from_sector(struct disk * hd,void * buf,uint8_t sec_cnt)
{
    uint32_t size_in_byte;
    if(sec_cnt == 0)
    {
        size_in_byte = 256 * 512;
    }
    else
    {
        size_in_byte =sec_cnt * 512;
    }
    insw(reg_data(hd->my_channel),buf,size_in_byte/2);////因为他的读写寄存器是16位
}

// 将buf中sec_cnt扇区的数据写入硬盘
static void write2sector(struct disk * hd,void * buf,uint8_t sec_cnt)
{
    uint32_t size_in_byte;
    if(sec_cnt == 0)
    {
        size_in_byte = 256 * 512;
    }
    else
    {
        size_in_byte = sec_cnt * 512;
    }
    //因为他的读写寄存器是16位
    outsw(reg_data(hd->my_channel),buf,size_in_byte/2);
}

//等待30秒
static bool busy_wait(struct disk * hd)
{
    struct ide_channel * channel = hd->my_channel;
    uint16_t time_limit = 30 *1000; // 可以等待30s
    while (time_limit -= 10 >= 0)
    {
        if(!(inb(reg_status(channel)) & BIT_ALT_STAT_BSY))
        {
            //返回数据是否就绪
            return (inb(reg_status(channel)) & BIT_ALT_STAT_DRQ);
        }
        else
        {
            mtime_sleep(10);
        }
    }
    return false;   //超时失败
}

// 从硬盘读取sec_cnt个扇区到buf
void ide_read(struct disk * hd,uint32_t lba,void * buf,uint32_t sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);    // 获取这个channel的锁
    slect_disk(hd);                         // 1. 选择设备

    uint32_t secs_op;                       //每次操作的扇区数
    uint32_t secs_done = 0;                 //已完成的扇区数
    while (secs_done < sec_cnt)
    {
        if((secs_done + 256) <= sec_cnt)
        {
            secs_op = 256;
        }
        else
        {
            secs_op = sec_cnt - secs_done;
        }

        // 2. 设置要操作的扇区数
        select_sector(hd,lba + secs_done,secs_op);
        // 3. 执行命令
        cmd_out(hd->my_channel,CMD_READ_SECTOR);

        // 阻塞自己的时机，如果硬盘已经发送命令，那就要阻塞自己，等待硬盘ready了，拿就可以释放了
        sema_down(&hd->my_channel->disk_done);

        // 4.检测硬盘是否可读
        if(!busy_wait(hd))
        {
            char error[64];
            sprintf(error,"%s read sector %d failed !!!\n",hd->name,lba);
            PANIC(error);
        }
        // 5.把数据从硬盘的缓冲区中读出
        read_from_sector(hd,(void*)((uint32_t)buf+secs_done*512),secs_op);
        secs_done += secs_op;
    }
    
    lock_release(&hd->my_channel->lock);
}

// 将buf中的sec_cnt扇区数据写入硬盘
void ide_write(struct disk * hd,uint32_t lba,void * buf,uint32_t sec_cnt)
{
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);

    // 1. 选择操作硬盘
    slect_disk(hd);

    uint32_t secs_op;       //每次操作的扇区数
    uint32_t secs_done = 0; //已完成的扇区数
    while(secs_done < sec_cnt)
    {
        if((secs_done + 256) <= sec_cnt)
        {
            secs_op = 256;
        }
        else
        {
            secs_op = sec_cnt - secs_done;
        }

        // 2. 写入待写入的扇区数和起始号
        select_sector(hd,lba + secs_done,secs_op);
        // 3.执行的命令写入reg_cmd
        cmd_out(hd->my_channel,CMD_WRITE_SECTOR);

        // 4.检测硬盘状态是否可写
        if(!busy_wait(hd))
        {
            char error[64];
            sprintf(error,"%s write sector %d failed !!\n",hd->name,lba);
            PANIC(error);
        }
        
        // 5 将数据写入硬盘
        write2sector(hd,(void *)((uint32_t)buf + secs_done * 512),secs_op);
        // 硬盘没回复就等待
        sema_down(&hd->my_channel->disk_done);
        secs_done += secs_op;
    }   
    lock_release(&hd->my_channel->lock);
}


//将dst中len个相邻字节交换后存入buf (因为都是以字来传输的)
static void swap_pairs_bytes(const char * dst,char * buf,uint32_t len)
{
    uint8_t idx;
    for(idx = 0;idx < len;idx += 2)
    {
        buf[idx + 1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx]='\0';
}


// 获得硬盘参数信息
static void identify_disk(struct disk * hd)
{
    char id_info[512];
    slect_disk(hd);
    cmd_out(hd->my_channel,CMD_IDENTIFY);
    //发送命令后要阻塞字节，等待中断接收后信号量
    sema_down(&hd->my_channel->disk_done);

    //醒来后执行下面代码
    if(!busy_wait(hd))
    {
        char error[64];
        sprintf(error,"%s identify failed!!!\n",hd->name);
        PANIC(error);
    }
    // 发送命令后，从data端口读取1扇区大小的内容
    read_from_sector(hd,id_info,1);

    char buf[64]={0};
    //identify得到的信息，偏移10字是sn号，长度20字节
    //                   偏移27字是硬盘型号，超度40
    //                   偏移60字是可使用的扇区数，长度2
    uint8_t sn_start = 10 * 2,sn_len = 20,md_start = 27 * 2,md_len = 40;
    swap_pairs_bytes(&id_info[sn_start],buf,sn_len);
    printk("    disk %s info:        SN: %s\n",hd->name,buf);
    // printk("hellp ");
    memset(buf,0,sizeof(buf));
    swap_pairs_bytes(&id_info[md_start],buf,md_len);
    printk("     MODLUE: %s\n",buf);

    uint32_t sectors = *(uint32_t*)&id_info[60*2];
    printk("     SECTORS: %d\n",sectors);
    printk("     CAPACITY: %dMB\n",sectors * 512 / 1024 / 1024);
}



// 扫描硬盘hd中地址为ext_lba的扇区中的所有分区
static void partition_scan(struct disk * hd,uint32_t ext_lba)
{
    // return;
    struct boot_sector * bs = sys_malloc(sizeof(struct boot_sector));
    ide_read(hd,ext_lba,bs,1);
    uint8_t part_idx = 0;
    struct partition_table_entry * p = bs->partition_table;

    // 遍历分区表项
    while((part_idx++) < 4)
    {
        if(p->fs_type == 0x5)   //若为扩展分区，总扩展或者 下一个扩展分区的标识都是0x5,这种不需要解析，要进一步读取它的EBR中的分区表）
        {
            if(ext_lba_base != 0)
            {
                partition_scan(hd,p->start_lba + ext_lba_base);
            }
            else
            {
                ext_lba_base = p->start_lba;
                partition_scan(hd,ext_lba_base);
            }
        }
        else if(p->fs_type != 0) //若是有效分区
        {
            if(ext_lba == 0)    //此时全是主分区
            {
                hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
                hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
                hd->prim_parts[p_no].my_disk = hd;
                list_append(&partition_list,&hd->prim_parts[p_no].part_tag);
                sprintf(hd->prim_parts[p_no].name,"%s%d",hd->name,p_no+1);
                p_no++;
                ASSERT(p_no < 4);   // 0 1 2 3
            }
            else
            {
                hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
                hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
                hd->logic_parts[l_no].my_disk = hd;
                list_append(&partition_list,&hd->logic_parts[l_no].part_tag);
                sprintf(hd->logic_parts[l_no].name,"%s%d",hd->name,l_no + 5);
                l_no++;
                if(l_no >= 8) return;
            }
        }
        p++;
    }
    sys_free(bs);
}

// 打印分区信息
static  bool partition_info(struct list_elem * pelem,int arg)
{
    struct partition * part = elem2entry(struct partition,part_tag,pelem);
    printk(" %s start_lba:0x%x,sec_cnt:0x%x\n",part->name,part->start_lba,part->sec_cnt);
    //只是为了主调函数list_traversal遍历元素
    return false;
}


// 硬盘中断处理程序
void intr_hd_handler(uint8_t irq_no)
{
    ASSERT(irq_no == 0x2e || irq_no == 0x2f); // 硬盘中断
    uint8_t ch_no = irq_no - 0x2e;            // channel号
    struct ide_channel * channel = &channels[ch_no];
    // if(channel->irq_no != irq_no)
    // {
    //     printk("iqr: %d -- %d",channel->irq_no,irq_no);
    // }
    ASSERT(channel->irq_no == irq_no);

    if(channel->expecting_intr)
    {
        channel->expecting_intr = false;
        sema_up(&channel->disk_done);       //执行完了就不要阻塞了

        inb(reg_status(channel));           // 读硬盘控制器状态，硬盘就受到认为可以执行新的读写了
    }
}


//应盘数据结构初始化
void ide_init()
{
    printk("ide_init start\n");
    uint8_t hd_cnt = *((uint8_t*)(0x475));  //获取硬盘数量
    ASSERT(hd_cnt > 0);

    channel_cnt = DIV_ROUND_UP(hd_cnt,2);   //有几个通道数

    struct ide_channel * channel;
    uint8_t channel_no = 0;         //通道0开始
    uint8_t dev_no = 0;
    list_init(&partition_list);

    while (channel_no < channel_cnt)
    {
        channel = &channels[channel_no];
        sprintf(channel->name,"ide%d",channel_no);  //ide n
        //为了每个ide通道初始化端口基址及其中断向量
        switch (channel_no)
        {
        case 0:
            channel->port_base = 0x1f0;
            channel->irq_no = 0x20 + 14;
            break;
        case 1:
            channel->port_base = 0x170;
            channel->irq_no = 0x20 + 15;
            break;           
        default:
            break;
        }
        
        // 注册盘的中断处理
        register_handler(channel->irq_no,intr_hd_handler);
        channel->expecting_intr = false;
        lock_init(&channel->lock);
        sema_init(&channel->disk_done,0);
        // 分别获取两个硬盘的参数及分区信息
        while(dev_no < 2)
        {
            struct disk * hd = &channel->devices[dev_no];
            hd->my_channel = channel;
            hd->dev_no = dev_no;
            sprintf(hd->name,"sd%c",'a'+channel_no * 2 + dev_no);
            identify_disk(hd);
                    
            if(dev_no != 0)
            {
                partition_scan(hd,0);
            }
            p_no = 0,l_no = 0;
            dev_no++;
        }

        dev_no = 0;
        channel_no++;
    }

    printk("\n all partition info\n");
    list_traversal(&partition_list,partition_info,(int)NULL);

    printk("ide_init done\n");

}