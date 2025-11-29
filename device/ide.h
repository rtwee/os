#ifndef __DEBICE_IDE_H__
#define __DEBICE_IDE_H__
#include "../lib/stdint.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/bitmap.h"
#include "../thread/sync.h"

struct disk;
struct ide_channel;

//分区结构
struct partition
{
    uint32_t start_lba;         //起始扇区数
    uint32_t sec_cnt;           //扇区数
    struct disk * my_disk;      //分区所属的硬盘
    struct list_elem part_tag;  //用于队列中的标记
    char name[8];               //分区名称
    struct super_block * sb;    //本分区的超级块
    struct bitmap block_bitmap; //快位图
    struct bitmap inode_bitmap; //inode位图
    struct list open_inodes;    //本分区打开的inode队列
};

// 硬盘结构
struct disk
{
    char name[8];                       //硬盘名称
    struct ide_channel * my_channel;    //此块硬盘归属于哪个ide通道
    uint8_t dev_no;                     //本硬盘是主0，还是从1
    struct partition prim_parts[4];     //主分区最多四个
    struct partition logic_parts[8];    //逻辑分区无限个，但是我们只实现8个
};


//ata通道结构
struct ide_channel
{
    char name[8];               //本ata通道的名称
    uint16_t port_base;         //本通道的起始端口号
    uint8_t irq_no;             //本通道的中断号
    struct lock lock;           //通道锁
    bool expecting_intr;        //表示等待硬盘中断
    struct semaphore disk_done; //用于阻塞、唤醒驱动程序(占用通道后，已经发出了命令，那就不能再发送了)
    struct disk devices[2];     //通道上两个盘
};

void ide_init();
void ide_read(struct disk * hd,uint32_t lba,void * buf,uint32_t sec_cnt);
void ide_write(struct disk * hd,uint32_t lba,void * buf,uint32_t sec_cnt);

#endif