#ifndef __KERNEL_MEMORY_H__
#define __KERNEL_MEMORY_H__
#include "stdint.h"
#include "bitmap.h"
#include "../lib/kernel/list.h"
enum pool_flags
{
    PF_KERNEL = 1,
    PF_USER   = 2
};

#define PG_P_1 1
#define PG_P_0 0
#define PG_RW_R 0
#define PG_RW_W 2
#define PG_US_S 0
#define PG_US_U 4


#define DESC_CNT 7  //arena种类是7个 16，32，64，128，256，512，1024

struct virtual_addr
{
    struct bitmap vaddr_bitmap; //虚拟地址管理用的位图
    uint32_t vaddr_start;       //起始的虚拟地址
};


// 内存块
struct mem_block
{
    struct list_elem free_elem;
};

// 内存块描述符
struct mem_block_desc
{
    uint32_t block_size;
    uint32_t blocks_per_arena;  //每个arena可以有多少个block
    struct list free_list;
};

extern struct pool kernel_pool,user_pool;
void mem_init();
void * get_kernel_pages(uint32_t pg_cnt);
void * get_a_page(enum pool_flags pf,uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc * desc_array);
void * sys_malloc(uint32_t size);
void mfree_page(enum pool_flags pf,void * _vaddr,uint32_t pg_cnt);
void sys_free(void * ptr);

#endif