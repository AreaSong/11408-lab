#include "order.h"

/*
 * 页在不在，问拉链散列表，期望是 O(1)。
 * 谁更老，问双向链表。lib/16 的结点只存关键字，不存链表指针，
 * 所以命中后还要按页号在链表里定位再挪到表头，定位是 O(页框数)。
 * 考场上页框很少，这点扫描不影响置换结果。
 */

static int hash_has(const HashTab *t, int page) {
    ChainWalk w;
    return chain_find(t, page, &w) == HASH_OK;
}

static int hash_add(HashTab *t, int page) {
    ChainWalk w;
    return chain_insert(t, page, &w) == HASH_OK;
}

static void hash_del(HashTab *t, int page) {
    ChainWalk w;
    chain_delete(t, page, &w);
}

int order_open(Order *o, int front_mru) {
    /* 61 是表长也是除数。页号可以比 61 大，冲突落到同一条链上。 */
    o->tab = hash_create(KIND_CHAIN, 61, 61, 1, 0);
    o->front_mru = front_mru;
    if (o->tab == NULL || !dlist_init(&o->list, false)) {
        hash_free(o->tab);
        o->tab = NULL;
        return 0;
    }
    return 1;
}

void order_close(Order *o) {
    if (o == NULL) {
        return;
    }
    dlist_destroy(&o->list);
    hash_free(o->tab);
    o->tab = NULL;
}

int order_has(const Order *o, int page) {
    return hash_has(o->tab, page);
}

int order_admit(Order *o, int page) {
    int pos;

    if (!hash_add(o->tab, page)) {
        return 0;
    }
    pos = o->front_mru ? 1 : dlist_length(&o->list) + 1;
    if (!dlist_insert(&o->list, pos, page)) {
        hash_del(o->tab, page);
        return 0;
    }
    return 1;
}

int order_hit(Order *o, int page) {
    int pos;

    if (!order_has(o, page)) {
        return 0;
    }
    /* FIFO 命中不更新顺序。谁先进来谁先走，和后来用过没有没关系。 */
    if (!o->front_mru) {
        return 1;
    }
    pos = dlist_locate(&o->list, page);
    if (pos <= 1) {
        return pos == 1;
    }
    if (!dlist_delete(&o->list, pos, NULL)) {
        return 0;
    }
    return dlist_insert(&o->list, 1, page);
}

int order_evict(Order *o, int *page) {
    int pos = o->front_mru ? dlist_length(&o->list) : 1;
    int e = 0;

    if (!dlist_delete(&o->list, pos, &e)) {
        return 0;
    }
    hash_del(o->tab, e);
    *page = e;
    return 1;
}

int order_len(const Order *o) {
    return dlist_length(&o->list);
}

int order_at(const Order *o, int pos) {
    int e = -1;
    if (!dlist_get(&o->list, pos, &e)) {
        return -1;
    }
    return e;
}

int order_links_ok(const Order *o) {
    return o != NULL && dlist_links_ok(&o->list);
}
