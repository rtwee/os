#include "ide.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../kernel/debug.h"
#include "../lib/stdio.h"

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


//应盘数据结构初始化
void ide_init()
{
    printk("ide_init start\n");
    uint8_t hd_cnt = *((uint8_t*)(0x475));  //获取硬盘数量
    ASSERT(hd_cnt > 0);

    channel_cnt = DIV_ROUND_UP(hd_cnt,2);   //有几个通道数

    struct ide_channel * channel;
    uint8_t channel_no = 0;         //通道0开始

    while (channel_no < channel_cnt)
    {
        channel = &channel[channel_no];
        vsprintf(channel->name,"ide%d",channel_no);  //ide n
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

        channel->expecting_intr = false;
        lock_init(&channel->lock);
        sema_init(&channel->disk_done,0);
        channel_no++;
    }
    printk("ide_init done\n");

}