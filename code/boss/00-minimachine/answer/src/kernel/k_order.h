#ifndef K_ORDER_H
#define K_ORDER_H

#include "dlist.h"
#include "hash.h"

/* front_mru 为 1 时位置 1 是刚用过的页。为 0 时位置 1 是最早进来的页。 */
typedef struct {
    DList list;
    HashTab *tab;
    int front_mru;
} KOrder;

int k_order_open(KOrder *o, int front_mru);
void k_order_close(KOrder *o);
int k_order_has(const KOrder *o, int page);
int k_order_admit(KOrder *o, int page);
int k_order_hit(KOrder *o, int page);
int k_order_drop(KOrder *o, int page);
int k_order_len(const KOrder *o);
int k_order_at(const KOrder *o, int pos);
int k_order_links(const KOrder *o);

#endif
