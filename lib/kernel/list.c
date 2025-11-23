#include "list.h"
#include "interrupt.h"
#include "stdint.h"

void list_init(struct list *plist)
{   
    plist->head.prev=NULL;
    plist->head.next=&plist->tail;
    plist->tail.prev=&plist->head;
    plist->tail.next=NULL;
}

void list_insert_before(struct list_elem * before,struct list_elem * elem)
{
    enum intr_status old_status = intr_disable();
    before->prev->next = elem;
    elem->prev = before->prev;
    elem->next = before;
    before->prev=elem;


    intr_set_intr(old_status);
}

//添加元素到头部
void list_push(struct list * plist,struct list_elem * elem)
{
    list_insert_before(plist->head.next,elem);
}
//添加元素到尾部
void list_append(struct list * plist,struct list_elem * elem)
{
    list_insert_before(&plist->tail,elem);
}

void list_remove(struct list_elem * pelem)
{
    enum intr_status old_status = intr_disable();

    pelem->prev->next=pelem->next;
    pelem->next->prev=pelem->prev;

    intr_set_intr(old_status);
}

//返回第一个元素并返回
struct list_elem * list_pop(struct list * plist)
{
    struct list_elem * elem = plist->head.next;
    list_remove(elem);
    return elem;
}

//在链表中寻找这个元素
bool elem_find(struct list * plist,struct list_elem * obj_elem)
{
    struct list_elem * elem = plist->head.next;
    while(elem != &plist->tail)
    {
        if(elem == obj_elem) return true;
        elem = elem->next;
    }
    return false;
}


struct list_elem * list_traversal(struct list * plist,function func,int arg)
{
    struct list_elem * elem = plist->head.next;
    if(list_empty(plist)) return NULL;
    while(elem != &plist->tail)
    {
        if(func(elem,arg)) return elem;
        elem = elem->next;
    }
    return NULL;
}

uint32_t list_len(struct list * plist)
{
    uint32_t len = 0;
    struct list_elem * elem = plist->head.next;
    while(elem != &plist->tail)
    {
        ++len;
        elem = elem->next;
    }
    return len;

}


bool list_empty(struct list * plist)
{
    return (plist->head.next == &plist->tail);
}
// void list_iterate(struct list * plist);







