#ifndef __FS_INODE_H__
#define __FS_INODE_H__

#include "../lib/stdint.h"
#include "../lib/kernel/list.h"
//inode结构
struct inode
{
    uint32_t i_no;          //inode编号
    uint32_t i_size;        //文件时，标识文件大小，目录时时所有目录项的大小和
    uint32_t i_open_cnts;   //文件被打开的次数
    bool write_deny;        //写文件不能并行，因此写之前要检查

    uint32_t i_sectors[13]; //0-11是直接块，12是间接块
    struct list_elem inode_tag;
};

#endif