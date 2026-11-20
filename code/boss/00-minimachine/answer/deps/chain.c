#include "hash.h"

#include <stdlib.h>

/*
 * 头插和王道的实现一致。一条链上各关键字的比较次数总是 1..L，
 * 所以成功 ASL 与头插还是尾插无关；某个关键字比几次则和次序有关。
 * 失败时只数关键字比较，空链记 0。空指针再算一次是另一种口径，由 count_null 单独给出。
 */
int chain_insert(HashTab *t, int key, ChainWalk *w) {
    int h = hash_addr(t, key);
    HashNode *p;
    HashNode *node;
    w->h = h;
    w->npath = 0;
    w->cmp = 0;
    for (p = t->head[h]; p != NULL; p = p->next) {
        w->key[w->npath++] = p->key;
        w->cmp++;
        if (p->key == key) {
            w->rc = HASH_DUP;
            return HASH_DUP;
        }
    }
    node = malloc(sizeof *node);
    if (node == NULL) {
        w->rc = HASH_FULL;
        return HASH_FULL;
    }
    node->key = key;
    node->next = t->head[h];
    t->head[h] = node;
    t->n++;
    w->cmp = 1;
    w->rc = HASH_OK;
    return HASH_OK;
}

int chain_find(const HashTab *t, int key, ChainWalk *w) {
    int h = hash_addr(t, key);
    HashNode *p;
    int cmp = 0;
    w->h = h;
    w->npath = 0;
    for (p = t->head[h]; p != NULL; p = p->next) {
        w->key[w->npath++] = p->key;
        cmp++;
        if (p->key == key) {
            w->cmp = cmp;
            w->rc = HASH_OK;
            return HASH_OK;
        }
    }
    w->cmp = cmp;
    w->rc = HASH_MISS;
    return HASH_MISS;
}

int chain_delete(HashTab *t, int key, ChainWalk *w) {
    int h = hash_addr(t, key);
    HashNode **link = &t->head[h];
    int cmp = 0;
    w->h = h;
    w->npath = 0;
    while (*link != NULL) {
        HashNode *p = *link;
        w->key[w->npath++] = p->key;
        cmp++;
        if (p->key == key) {
            *link = p->next;
            free(p);
            t->n--;
            w->cmp = cmp;
            w->rc = HASH_OK;
            return HASH_OK;
        }
        link = &p->next;
    }
    w->cmp = cmp;
    w->rc = HASH_MISS;
    return HASH_MISS;
}
