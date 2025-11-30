#ifndef __FS_FS_H__
#define __FS_FS_H__

#include "inode.h"
#include "super_block.h"
#include "../device/ide.h"

#define MAX_FILES_PER_PART 4096     //最大文件数
#define BITS_PER_SECTOR 4096        //每个扇区的bit数量 512*8 = 4096
#define SECTOR_SIZE 512             //扇区大小
#define BLOCK_SIZE  SECTOR_SIZE     //块大小，我们和扇区大小一致，但是真实的一般会大一些

//文件系统类型
enum file_types
{
    FT_UNKNOWN,     //unknown
    FT_REGULAR,     //文件
    FT_DIRECTORY    //目录
};
void partition_format(struct partition * part);
void filesys_init();

#endif