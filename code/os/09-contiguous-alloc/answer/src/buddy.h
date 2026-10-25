#ifndef BUDDY_H
#define BUDDY_H

#define BUDDY_MAX 64

typedef struct {
    long long start;
    long long size;
    int free;
    char name[16];
} BuddyBlk;

typedef struct {
    long long mem;
    BuddyBlk blk[BUDDY_MAX];
    int n;
} Buddy;

/* reason：1 申请比整块内存还大，2 没有足够大的空闲块。ok 为 1 时 reason 无意义。 */
typedef struct {
    int ok;
    int reason;
    long long at;
    long long size;
} BuddyPut;

int buddy_open(Buddy *b, long long mem);
int buddy_alloc(Buddy *b, const char *name, long long need, BuddyPut *out);
int buddy_free(Buddy *b, const char *name, int *merges);
int buddy_collect(const Buddy *b, int want_free, BuddyBlk *out, int cap);

#endif
