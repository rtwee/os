#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "bitmap.h"
#include "global.h"
#include "debug.h"
#include "string.h"
#include "../thread/sync.h"
#include "../thread/thread.h"
#include "../kernel/interrupt.h"
#include "../lib/kernel/print.h"

#define PG_SIZE 4096
//我们的栈顶是0xc009f000,栈顶 0xc009e00-9efff里面放的是PCB  我们把a000-dfff这里全部放位图
#define MEM_BITMAP_BASE 0xc009a000
#define K_HEAP_START 0xc0100000 //虚拟的起始地址越过了低端的1M 也就是内核要用的位置

//根据虚拟地址来得到 pde 和 pte 的索引
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

//这个是物理的内存池
struct pool
{
    struct bitmap pool_bitmap;
    uint32_t phy_addr_start;
    uint32_t pool_size;
    struct lock lock;
};

// 内存仓库
struct arena
{
    struct mem_block_desc * desc;   //此arena关联的mem_bloc_desc;
    uint32_t cnt;   //large位true则表示是页框数量,否则是mem_block的数量
    bool large;
};

// desc 固定在内核的内存中
struct mem_block_desc k_block_desc[DESC_CNT]; //内核内存块描述符数组

struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr;


// 位malloc做准备
void block_desc_init(struct mem_block_desc * desc_array)
{
    uint16_t desc_idx,block_size = 16;
    for(desc_idx = 0; desc_idx < DESC_CNT;desc_idx++)
    {
        desc_array[desc_idx].block_size = block_size;
        desc_array[desc_idx].blocks_per_arena = (PG_SIZE - sizeof(struct arena))/block_size;
        list_init(&desc_array[desc_idx].free_list);
        block_size *= 2;
    }
}

//申请虚拟地址 是连续pg_cnt个页
static void * vaddr_get(enum pool_flags pf,uint32_t pg_cnt)
{
    int vaddr_start = 0,bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL)
    {
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);
        if(bit_idx_start == -1) return NULL;
        while(cnt < pg_cnt)
            bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start + cnt++,1);
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    }
    else
    {
        //用户的虚拟内存池 将来在用户进程中 补全
        struct task_struct * cur = running_thread();
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
        if(bit_idx_start == -1) return NULL;
        while(cnt < pg_cnt)
        {
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx_start + cnt++,1);
        }
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void *)vaddr_start;
}

//得到pde的指针
uint32_t * pde_ptr(uint32_t vaddr)
{
    //0xfffff000两次索引到 页目录表  再根据 索引锁定偏移位置
    uint32_t * pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

//得到pte的指针
uint32_t * pte_ptr(uint32_t vaddr)
{
    //0xffc00000先到自身  再根据 pde所在的位置就确定了pte  （可以理解为表只转了一次）
    uint32_t * pte = (uint32_t *)((0xffc00000) + ((0xffc00000 & vaddr) >> 10) + PTE_IDX(vaddr)*4);
    return pte;
}

//从物理内存池中得到一个物理页
static void * palloc(struct pool * m_pool)
{
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap,1);
    if(bit_idx == -1)return NULL;
    bitmap_set(&m_pool->pool_bitmap,bit_idx,1);
    // put_str("bitmap set idx: ");
    // put_int(bit_idx);
    // put_str("bitmap is 0x");
    // put_int(m_pool->pool_bitmap.bits[bit_idx/8]);
    // put_str("\n");
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void *)page_phyaddr;
}

//页表中添加虚拟地址 _vaddr 到物理地址 phyaddr的映射
static void page_table_add(void * _vaddr,void * _page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t page_phyaddr = (uint32_t)_page_phyaddr;

    uint32_t * pde = pde_ptr(vaddr);
    uint32_t * pte = pte_ptr(vaddr);

    //在没有创建 页表时 也就是对应的pde里面的内容是空的
    
    if(*pde & 0x00000001)
    {
        //现在说明这个页表是存在的
        ASSERT(!(*pte & 0x00000001));//判断页目录项是否存在
        if(!(*pte & 0x00000001))    
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        else
        {
            PANIC("pte repeat");
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    }
    else
    {
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);

        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        //可以理解位pte 的这个页表 就是我们pde所指向的地方 ，那pte已经能够代表了 但是我们需要他的偏移是0
        memset((void *)((int)pte & 0xfffff000),0,PG_SIZE);//把这个映射的页表清空
        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

//分配pg_cnt个页空间，成功则返回起始虚拟地址
void * malloc_page(enum pool_flags pf,uint32_t pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);

    void * vaddr_start = vaddr_get(pf,pg_cnt);
    if(vaddr_start == NULL) return NULL;
    uint32_t vaddr = (uint32_t)vaddr_start,cnt = pg_cnt;
    struct pool * mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    
    //虚拟地址是连续的 但是物理地址可以不是连续的
    while(cnt-- > 0)
    {
        void * page_phyaddr = palloc(mem_pool);
        if(page_phyaddr == NULL)return NULL;
        page_table_add((void*)vaddr,page_phyaddr);
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}

//从内核中申请n页内存
void * get_kernel_pages(uint32_t pg_cnt)
{
    void * vaddr = malloc_page(PF_KERNEL,pg_cnt);
    if(vaddr != NULL)
        memset(vaddr,0,pg_cnt * PG_SIZE);
    return vaddr;
}


//在用户空间中申请4K内存，并返回其虚拟地址
void * get_user_pages(uint32_t pg_cnt)
{
    lock_acquire(&user_pool.lock);
    void * vaddr = malloc_page(PF_USER,pg_cnt);
    if(vaddr != NULL)
    memset(vaddr,0,pg_cnt*PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

//将地址vaddr与pf吃中的物理地址关联，仅支持一页空间分配
void * get_a_page(enum pool_flags pf,uint32_t vaddr)
{
    struct pool * mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    //先将虚拟地址对应的位图置1 
    struct task_struct * cur = running_thread();
    int32_t bit_idx = -1;
    //若当前是用户进程申请的用户内存，就修改用户进程自己的虚拟地址位图
    if(cur->pgdir != NULL && pf == PF_USER)
    {
        bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start)/PG_SIZE; //在位图中的索引
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx,1);
    }
    else if(cur->pgdir == NULL && pf == PF_KERNEL)
    {
        bit_idx = (vaddr - kernel_vaddr.vaddr_start)/PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx,1);
    }
    else
    {
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    }
    void * page_phyaddr = palloc(mem_pool);
    if(page_phyaddr == NULL) return NULL;
    page_table_add((void*)vaddr,page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

//得到虚拟地址 映射到物理地址
uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t * pte = pte_ptr(vaddr);
    //去掉低12位的页表属性+虚拟地址vaddr的低12位
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

//初始化内存池
static void mem_pool_init(uint32_t all_mem)
{
    put_str(" mem_pool_init start\n");
    uint32_t page_table_size = PG_SIZE * 256;   //当时创建了256个页表用来映射

    uint32_t used_mem = page_table_size + 0x100000;
    uint32_t free_mem = all_mem - used_mem;
    uint16_t all_free_page = free_mem / PG_SIZE;

    uint16_t kernel_free_pages = all_free_page / 2;
    uint16_t user_free_pages   = all_free_page - kernel_free_pages;

    //余数不做处理
    uint32_t kbm_length = kernel_free_pages / 8;
    uint32_t ubm_length = user_free_pages / 8;

    //kernel pool start
    uint32_t kp_start = used_mem;
    //user pool start
    uint32_t up_start = used_mem + kernel_free_pages*PG_SIZE;

    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start = up_start;
    //这块这个空间是不是可以修改 有点bug
    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len=ubm_length;

    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length);

    put_str("-------------memory info-------------\n");
    put_str("    kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str("  kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);put_str("\n");

    put_str("    user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str("  user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);put_str("\n");
    put_str("--------------------------------------\n");
    //将位图置0
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    //初始化内核的虚拟地址位图
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;

    kernel_vaddr.vaddr_bitmap.bits = (void *)(MEM_BITMAP_BASE + kbm_length + ubm_length);//紧接着这两个位图

    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);
    put_str(" mem_pool_init done\n");
}

void mem_init()
{
    put_str("--------------------memory init--------------------\n");
    uint32_t mem_bytes_total = (*(uint32_t *)(0xb00));
    mem_pool_init(mem_bytes_total);
    block_desc_init(k_block_desc);  //初始化mem_block_desc数组，为malloc做准备
    put_str("------------------------done-----------------------\n");
}

// 将物理地址pg_phy_addr 回收到物理内存池
void pfree(uint32_t pg_phy_addr)
{
    struct pool * mem_pool;
    uint32_t bit_idx = 0;
    if(pg_phy_addr >= user_pool.phy_addr_start)
    {
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    }
    else
    {
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap,bit_idx,0);//位清零
}

// 去掉页表中虚拟地址vaddr的映射，我们只是取消了vaddr对应pte的映射
static void page_table_pte_remove(uint32_t vaddr)
{
    uint32_t * pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;
    asm volatile("invlpg %0"::"m"(vaddr):"memory"); // 重新加载vaddr(刷新 tlb。)
}

// 在虚拟地址池中释放_vaddr 起始的连续pg_cnt个虚拟页地址
static void vaddr_remove(enum pool_flags pf,void * _vaddr,uint32_t pg_cnt)
{
    uint32_t bit_idx_start = 0,vaddr = (uint32_t)_vaddr,cnt = 0;

    if(pf == PF_KERNEL)
    {
        bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        while ((cnt < pg_cnt))
        {
            bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start + cnt++,0);
        }
    }
    else
    {
        struct task_struct * cur_thread = running_thread();
        bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start)/PG_SIZE;
        while ((cnt < pg_cnt))
        {
            bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap,bit_idx_start + cnt++,0);
        }
    }
    
}

// 释放虚拟地址vaddr为起始的cnt个物理地址
void mfree_page(enum pool_flags pf,void * _vaddr,uint32_t pg_cnt)
{
    uint32_t pg_phy_addr;
    uint32_t vaddr = (int32_t)_vaddr,page_cnt = 0;
    ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);
    // 获取虚拟地址对应的物理地址
    pg_phy_addr = addr_v2p(vaddr);

    // 确保释放的物理内存在低端1M+1KB大小的页目录+1K大小的页表地址之外
    ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);

    //判断pg_phy_addr属于用户物理内存池 还是 内核物理内存池
    if(pg_phy_addr >= user_pool.phy_addr_start)
    {
        vaddr -= PG_SIZE;
        while(page_cnt < pg_cnt)
        {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);

            // 先把物理地址归还
            pfree(pg_phy_addr);
            // 在从页表中清楚pte
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        vaddr_remove(pf,_vaddr,pg_cnt);
    }
    else
    {
        vaddr -= PG_SIZE;
        while(page_cnt < pg_cnt)
        {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= kernel_pool.phy_addr_start && pg_phy_addr < user_pool.phy_addr_start);

            // 先把物理地址归还
            pfree(pg_phy_addr);
            // 在从页表中清楚pte
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        vaddr_remove(pf,_vaddr,pg_cnt);

    }

}


// 返回arena中第idx个内存块地址
static struct mem_block * arena2block(struct arena * a,uint32_t idx)
{
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

// 返回内存块b所在的arena地址
static struct arena * block2arena(struct mem_block * b)
{
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

// 在堆中申请size字节内存
void * sys_malloc(uint32_t size)
{
    enum pool_flags PF;
    struct pool * mem_pool;
    uint32_t pool_size;
    struct mem_block_desc *desc;
    struct task_struct * cur_thread = running_thread();

// 判断用哪个内存池
    if(cur_thread->pgdir == NULL)
    {
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        desc = k_block_desc;
    }
    else
    {
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        desc = cur_thread->u_block_desc;
    }

    // 若申请的内存不在内存池容量范围内，直接返回NULL
    if(!(size > 0 && size < pool_size)) return NULL;

    struct arena * a;
    struct mem_block * b;
    lock_acquire(&mem_pool->lock);

    // 超过最大内存块1024,就分配页框
    if(size > 1024)
    {
        //算放元数据，向上取整需要的页数
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena),PG_SIZE);

        a = malloc_page(PF,page_cnt);
        if(a != NULL)
        {
            memset(a,0,page_cnt * PG_SIZE);

            //对于分配的大块页框，将desc置为NULL，cnt为页框数,large为true
            a->desc = NULL;
            a->large = true;
            a->cnt = page_cnt;  //使用大页时，cnt是页数
            lock_release(&mem_pool->lock);
            return (void *)(a + 1); // 跳过了 arene元数据,返回的可用内存处
        }
        else
        {
            lock_release(&mem_pool->lock);
            return NULL;
        }
    }
    else    //说明申请内存小于1024 // 适配对应的mem_block_desc
    {
        uint8_t desc_idx;

        for ( desc_idx = 0; desc_idx < DESC_CNT; desc_idx++)
        {
            if(size <= desc[desc_idx].block_size)
            {
                break;
            }
        }
        if(list_empty(&desc[desc_idx].free_list))
        {
            a = malloc_page(PF,1);
            if(a == NULL)
            {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a,0,PG_SIZE);

            // 对于分配的小内存块，将desc置为对应内存块描述符
            a->desc = &desc[desc_idx]; // 设置了mem_block_desc
            a->large = false;
            a->cnt = desc[desc_idx].blocks_per_arena;

            uint32_t block_idx;

            enum intr_status old_status = intr_disable();
            for(block_idx = 0; block_idx < desc[desc_idx].blocks_per_arena;++block_idx)
            {
                b = arena2block(a,block_idx);
                ASSERT(!elem_find(&a->desc->free_list,&b->free_elem));
                list_append(&a->desc->free_list,&b->free_elem);
            }
            
            intr_set_intr(old_status);
        }
        // 开始分配内存块
        b = elem2entry(struct mem_block,free_elem,list_pop(&(desc[desc_idx].free_list)));

        a = block2arena(b);
        a->cnt--;
        lock_release(&mem_pool->lock);
        return (void*)b;
        
    }

    lock_release(&mem_pool->lock);
    return NULL;
}

// 回收ptr
void sys_free(void * ptr)
{
    ASSERT(ptr != NULL);
    if (ptr != NULL)
    {
        enum pool_flags PF;
        struct pool * mem_pool;

        //判断是线程还是进程
        if (running_thread()->pgdir == NULL)
        {
            PF = PF_KERNEL;
            mem_pool = &kernel_pool;
        }
        else
        {
            PF = PF_USER;
            mem_pool = &user_pool;
        }

        lock_acquire(&mem_pool->lock);
        struct mem_block * b = ptr;
        struct arena * a = block2arena(b);

        ASSERT(a->large == false || a->large == true);
        if(a->desc == NULL && a->large == true)
        {
            mfree_page(PF,a,a->cnt); // 按照页框大小进行释放
        }
        else    //小于等于1024的mem_block
        {
            list_append(&a->desc->free_list,&b->free_elem); // 重新收回

            // 再判断此arena中 的内存块是否都是空闲的
            if(++a->cnt == a->desc->blocks_per_arena)
            {
                uint32_t block_idx;
                for(block_idx = 0;block_idx < a->desc->blocks_per_arena;++block_idx)
                {
                    struct mem_block * b = arena2block(a,block_idx);
                    ASSERT(elem_find(&a->desc->free_list,&b->free_elem));
                    list_remove(&b->free_elem);
                }
                mfree_page(PF,a,1);
            }
        }
        lock_release(&mem_pool->lock);
    }
    
}