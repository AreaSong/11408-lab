#include "kernel/k_order.h"

/*
 * 页在不在，问拉链散列表。谁更老，问双向链表。
 * lib/16 的结点只存关键字，所以命中后还要按页号在链表里定位。页框很少，这点扫描不改变淘汰结果。
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

int k_order_open(KOrder *o, int front_mru) {
    o->tab = hash_create(KIND_CHAIN, 61, 61, 1, 0);
    o->front_mru = front_mru ? 1 : 0;
    if (o->tab == NULL || !dlist_init(&o->list, false)) {
        hash_free(o->tab);
        o->tab = NULL;
        return 0;
    }
    return 1;
}

void k_order_close(KOrder *o) {
    if (o == NULL) {
        return;
    }
    dlist_destroy(&o->list);
    hash_free(o->tab);
    o->tab = NULL;
}

int k_order_has(const KOrder *o, int page) { return o != NULL && hash_has(o->tab, page); }

int k_order_admit(KOrder *o, int page) {
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

int k_order_hit(KOrder *o, int page) {
    int pos;
    if (!k_order_has(o, page)) {
        return 0;
    }
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
    return dlist_insert(&o->list, 1, page) ? 1 : 0;
}

int k_order_drop(KOrder *o, int page) {
    int pos = dlist_locate(&o->list, page);
    if (pos < 1 || !dlist_delete(&o->list, pos, NULL)) {
        return 0;
    }
    hash_del(o->tab, page);
    return 1;
}

int k_order_len(const KOrder *o) { return o == NULL ? 0 : dlist_length(&o->list); }

int k_order_at(const KOrder *o, int pos) {
    int value = -1;
    if (o == NULL || !dlist_get(&o->list, pos, &value)) {
        return -1;
    }
    return value;
}

int k_order_links(const KOrder *o) { return o != NULL && dlist_links_ok(&o->list); }
