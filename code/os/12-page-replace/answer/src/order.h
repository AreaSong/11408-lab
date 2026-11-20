#ifndef ORDER_H
#define ORDER_H

#include "dlist.h"
#include "hash.h"

/* front_mru 为 1：位置 1 是刚用过的页，表尾是 LRU 的牺牲页。
 * front_mru 为 0：位置 1 是最早进来的页，FIFO 从这里淘汰，命中不挪动。 */
typedef struct {
    DList list;
    HashTab *tab;
    int front_mru;
} Order;

int order_open(Order *o, int front_mru);
void order_close(Order *o);
int order_has(const Order *o, int page);
int order_admit(Order *o, int page);
int order_hit(Order *o, int page);
int order_evict(Order *o, int *page);
int order_len(const Order *o);
int order_at(const Order *o, int pos);
int order_links_ok(const Order *o);

#endif
