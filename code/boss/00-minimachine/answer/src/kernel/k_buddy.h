#ifndef K_BUDDY_H
#define K_BUDDY_H

#include "dlist.h"

/* 容量是 1 到 64 的 2 的幂。空闲块按大小分链，链是 lib/02 的双链表，链上只存起点。 */
#define K_BUDDY_ORDERS 7
#define K_BUDDY_JOBS 64

enum { K_BUDDY_TOO_BIG = 1, K_BUDDY_NO_BLOCK = 2 };

typedef struct {
    int start;
    int size;
    char name[16];
} KBuddyBlk;

typedef struct {
    int used;
    int start;
    int size;
    char name[16];
} KBuddyJob;

typedef struct {
    int ready;
    int cap;
    int norder;
    int njob;
    DList free_list[K_BUDDY_ORDERS];
    KBuddyJob job[K_BUDDY_JOBS];
} KBuddy;

int k_buddy_open(KBuddy *b, int cap);
void k_buddy_close(KBuddy *b);
int k_buddy_alloc(KBuddy *b, const char *name, int need, int *at, int *size);
int k_buddy_free(KBuddy *b, const char *name, int *merges);
int k_buddy_list(const KBuddy *b, int want_free, KBuddyBlk *out, int cap);
int k_buddy_links(const KBuddy *b);

#endif
