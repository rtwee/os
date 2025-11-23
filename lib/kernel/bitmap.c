#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

void bitmap_init(struct bitmap * btmp)
{
    memset(btmp->bits,0,btmp->btmp_bytes_len);
}

//判断bit_idx 位 是否为1
bool bitmap_scan_test(struct bitmap * btmp,uint32_t bit_idx)
{
    uint32_t byte_idx = bit_idx/8;
    uint32_t bit_odd = bit_idx % 8;
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd));
}

// //在为位图中申请  连续   cnt 个位 ，成功返回起始下标，失败返回-1
// int bitmap_scan(struct bitmap * btmp,uint32_t cnt)
// {
//     uint32_t idx_byte = 0;
//     while((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len))
//     {
//         idx_byte++; //ff说明这里全部都被分配完了
//     }
//     ASSERT(idx_byte < btmp->btmp_bytes_len);
//     if(idx_byte == btmp->btmp_bytes_len) return -1; //说明整个位图已经被占满了

//     //现在是某个字节范围内 有空位了

//     int idx_bit=0;
//     while((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte])
//     {
//         idx_bit++;
//     }
//     //现idx_bit就在那个空位上
//     int bit_idx_start = idx_byte * 8 + idx_bit; //这是位图整体的下标位置
//     if(cnt == 1) return bit_idx_start;


//     uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start); //这是从该位 到结束有多少空间
//     uint32_t next_bit = bit_idx_start + 1;
//     uint32_t count=1;                           //用来统计找到的空闲位的个数

//     bit_idx_start = -1; //先将其设置为-1，若找不到连续的位就直接返回
//     while(bit_left-- > 0)
//     {
//         if(!(bitmap_scan_test(btmp,next_bit)))
//         {
//             count++;
//         }
//         else
//         {
//             count = 0;      //这里没有连续的位 那只能从0开始重新找了
//         }
//         if(count == cnt)    //找到连续的 位了
//         {
//             bit_idx_start = next_bit - cnt + 1;
//             break;
//         }
//         next_bit++;
//     }


//     return bit_idx_start;

// }

int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {
    uint32_t idx_byte = 0;	 // 用于记录空闲位所在的字节
 /* 先逐字节比较,蛮力法 */
    while (( 0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)) {
 /* 1表示该位已分配,所以若为0xff,则表示该字节内已无空闲位,向下一字节继续找 */
       idx_byte++;
    }
 
    ASSERT(idx_byte < btmp->btmp_bytes_len);
    if (idx_byte == btmp->btmp_bytes_len) {  // 若该内存池找不到可用空间		
       return -1;
    }
 
  /* 若在位图数组范围内的某字节内找到了空闲位，
   * 在该字节内逐位比对,返回空闲位的索引。*/
    int idx_bit = 0;
  /* 和btmp->bits[idx_byte]这个字节逐位对比 */
    while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) { 
      idx_bit++;
    }
      
    int bit_idx_start = idx_byte * 8 + idx_bit;    // 空闲位在位图内的下标
    if (cnt == 1) {
       return bit_idx_start;
    }
 
    uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start);   // 记录还有多少位可以判断
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;	      // 用于记录找到的空闲位的个数
 
    bit_idx_start = -1;	      // 先将其置为-1,若找不到连续的位就直接返回
    while (bit_left-- > 0) {
       if (!(bitmap_scan_test(btmp, next_bit))) {	 // 若next_bit为0
      count++;
       } else {
      count = 0;
       }
       if (count == cnt) {	    // 若找到连续的cnt个空位
      bit_idx_start = next_bit - cnt + 1;
      break;
       }
       next_bit++;          
    }
    return bit_idx_start;
 }


//将btmp的bit_idx位设置为value
void bitmap_set(struct bitmap * btmp,uint32_t bit_idx,int8_t value)
{
    ASSERT((value == 0) || (value == 1));
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;

    if(value)
    {
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    }
    else
    {
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}