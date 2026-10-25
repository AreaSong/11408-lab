#include "buddy.h"

#include <string.h>

static void copy_name(char *dst, const char *src) {
    size_t i = 0;
    while (i < 15 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int is_pow2(long long n) {
    return n > 0 && (n & (n - 1)) == 0;
}

static long long ceil_pow2(long long n) {
    long long p = 1;
    if (n < 1) {
        return -1;
    }
    while (p < n) {
        if (p > (1LL << 62)) {
            return -1;
        }
        p <<= 1;
    }
    return p;
}

static int find_named(const Buddy *b, const char *name) {
    int i;
    for (i = 0; i < b->n; i++) {
        if (!b->blk[i].free && strcmp(b->blk[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_block(const Buddy *b, long long want) {
    int i;
    int best = -1;
    for (i = 0; i < b->n; i++) {
        if (!b->blk[i].free || b->blk[i].size < want) {
            continue;
        }
        if (best < 0 || b->blk[i].size < b->blk[best].size ||
            (b->blk[i].size == b->blk[best].size && b->blk[i].start < b->blk[best].start)) {
            best = i;
        }
    }
    return best;
}

static void split_down(Buddy *b, int index, long long want) {
    while (b->blk[index].size > want) {
        long long half = b->blk[index].size / 2;
        BuddyBlk *hi = &b->blk[b->n];
        hi->start = b->blk[index].start + half;
        hi->size = half;
        hi->free = 1;
        hi->name[0] = '\0';
        b->n++;
        b->blk[index].size = half;
    }
}

static void remove_blk(Buddy *b, int index) {
    memmove(&b->blk[index], &b->blk[index + 1],
            (size_t)(b->n - index - 1) * sizeof(BuddyBlk));
    b->n--;
}

static int find_buddy(const Buddy *b, long long start, long long size, int self) {
    long long bud = start ^ size;
    int i;
    for (i = 0; i < b->n; i++) {
        if (i == self) {
            continue;
        }
        if (b->blk[i].free && b->blk[i].start == bud && b->blk[i].size == size) {
            return i;
        }
    }
    return -1;
}

int buddy_open(Buddy *b, long long mem) {
    if (!is_pow2(mem) || mem > BUDDY_MAX) {
        return -1;
    }
    memset(b, 0, sizeof *b);
    b->mem = mem;
    b->n = 1;
    b->blk[0].start = 0;
    b->blk[0].size = mem;
    b->blk[0].free = 1;
    return 0;
}

int buddy_alloc(Buddy *b, const char *name, long long need, BuddyPut *out) {
    long long want;
    long long sz;
    int best;
    int splits = 0;
    memset(out, 0, sizeof *out);
    if (need <= 0) {
        return -2;
    }
    if (find_named(b, name) >= 0) {
        return -3;
    }
    if (need > b->mem) {
        out->reason = 1;
        return 0;
    }
    want = ceil_pow2(need);
    if (want < 0 || want > b->mem) {
        out->reason = 1;
        return 0;
    }
    best = find_block(b, want);
    if (best < 0) {
        out->reason = 2;
        return 0;
    }
    sz = b->blk[best].size;
    while (sz > want) {
        splits++;
        sz /= 2;
    }
    if (b->n + splits > BUDDY_MAX) {
        return -4;
    }
    split_down(b, best, want);
    b->blk[best].free = 0;
    copy_name(b->blk[best].name, name);
    out->ok = 1;
    out->at = b->blk[best].start;
    out->size = want;
    return 0;
}

int buddy_free(Buddy *b, const char *name, int *merges) {
    int index = find_named(b, name);
    if (index < 0) {
        return -1;
    }
    b->blk[index].free = 1;
    b->blk[index].name[0] = '\0';
    *merges = 0;
    for (;;) {
        int other = find_buddy(b, b->blk[index].start, b->blk[index].size, index);
        long long ns;
        long long nz;
        int keep;
        int drop;
        if (other < 0) {
            break;
        }
        ns = b->blk[index].start < b->blk[other].start ? b->blk[index].start
                                                      : b->blk[other].start;
        nz = b->blk[index].size * 2;
        keep = index < other ? index : other;
        drop = index < other ? other : index;
        remove_blk(b, drop);
        if (drop < keep) {
            keep--;
        }
        b->blk[keep].start = ns;
        b->blk[keep].size = nz;
        b->blk[keep].free = 1;
        b->blk[keep].name[0] = '\0';
        index = keep;
        (*merges)++;
    }
    return 0;
}

int buddy_collect(const Buddy *b, int want_free, BuddyBlk *out, int cap) {
    int idx[BUDDY_MAX];
    int n = 0;
    int i;
    int j;
    int copy;
    for (i = 0; i < b->n; i++) {
        if (b->blk[i].free == want_free) {
            idx[n++] = i;
        }
    }
    for (i = 1; i < n; i++) {
        int key = idx[i];
        j = i;
        while (j > 0 && b->blk[key].start < b->blk[idx[j - 1]].start) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = key;
    }
    copy = n < cap ? n : cap;
    for (i = 0; i < copy; i++) {
        out[i] = b->blk[idx[i]];
    }
    return n;
}
