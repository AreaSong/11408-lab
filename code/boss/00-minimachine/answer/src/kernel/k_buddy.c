#include "kernel/k_buddy.h"

#include <string.h>

static int is_pow2(int n) { return n > 0 && (n & (n - 1)) == 0; }

static int order_of(int size) {
    int n = 0;
    while (size > 1) {
        size >>= 1;
        n++;
    }
    return n;
}

static int ceil_pow2(int n) {
    int p = 1;
    if (n < 1) {
        return -1;
    }
    while (p < n) {
        if (p > 32) {
            return -1;
        }
        p <<= 1;
    }
    return p;
}

static void copy_name(char *dst, const char *src) {
    int i = 0;
    while (i < 15 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int insert_sorted(DList *list, int start) {
    int n = dlist_length(list);
    int i;
    for (i = 1; i <= n; i++) {
        int cur = 0;
        if (!dlist_get(list, i, &cur)) {
            return 0;
        }
        if (start < cur) {
            return dlist_insert(list, i, start) ? 1 : 0;
        }
    }
    return dlist_insert(list, n + 1, start) ? 1 : 0;
}

static int find_job(const KBuddy *b, const char *name) {
    int i;
    for (i = 0; i < b->njob; i++) {
        if (b->job[i].used && strcmp(b->job[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int job_slot(KBuddy *b) {
    int i;
    for (i = 0; i < b->njob; i++) {
        if (!b->job[i].used) {
            return i;
        }
    }
    if (b->njob >= K_BUDDY_JOBS) {
        return -1;
    }
    return b->njob++;
}

int k_buddy_open(KBuddy *b, int cap) {
    int i;
    if (b == NULL || !is_pow2(cap) || cap > 64) {
        return -1;
    }
    memset(b, 0, sizeof *b);
    b->cap = cap;
    b->norder = order_of(cap) + 1;
    for (i = 0; i < b->norder; i++) {
        if (!dlist_init(&b->free_list[i], false)) {
            while (--i >= 0) {
                dlist_destroy(&b->free_list[i]);
            }
            return -1;
        }
    }
    if (!insert_sorted(&b->free_list[b->norder - 1], 0)) {
        k_buddy_close(b);
        return -1;
    }
    b->ready = 1;
    return 0;
}

void k_buddy_close(KBuddy *b) {
    int i;
    if (b == NULL || b->norder <= 0) {
        return;
    }
    for (i = 0; i < b->norder; i++) {
        dlist_destroy(&b->free_list[i]);
    }
    b->ready = 0;
    b->norder = 0;
}

int k_buddy_alloc(KBuddy *b, const char *name, int need, int *at, int *size) {
    int want;
    int order;
    int start = 0;
    int got;
    int slot;
    if (b == NULL || !b->ready || name == NULL || name[0] == '\0' || at == NULL || size == NULL || need <= 0) {
        return -1;
    }
    if (find_job(b, name) >= 0) {
        return -1;
    }
    if (need > b->cap) {
        return K_BUDDY_TOO_BIG;
    }
    want = ceil_pow2(need);
    if (want < 0 || want > b->cap) {
        return K_BUDDY_TOO_BIG;
    }
    for (order = order_of(want); order < b->norder; order++) {
        if (!dlist_empty(&b->free_list[order])) {
            break;
        }
    }
    if (order >= b->norder || !dlist_delete(&b->free_list[order], 1, &start)) {
        return K_BUDDY_NO_BLOCK;
    }
    got = 1 << order;
    while (got > want) {
        int half = got / 2;
        got = half;
        if (!insert_sorted(&b->free_list[order_of(half)], start + half)) {
            return -1;
        }
    }
    slot = job_slot(b);
    if (slot < 0) {
        return -1;
    }
    b->job[slot].used = 1;
    b->job[slot].start = start;
    b->job[slot].size = got;
    copy_name(b->job[slot].name, name);
    *at = start;
    *size = got;
    return 0;
}

int k_buddy_free(KBuddy *b, const char *name, int *merges) {
    int index;
    int start;
    int size;
    if (b == NULL || !b->ready || name == NULL || merges == NULL) {
        return -1;
    }
    index = find_job(b, name);
    if (index < 0) {
        return -1;
    }
    start = b->job[index].start;
    size = b->job[index].size;
    b->job[index].used = 0;
    *merges = 0;
    for (;;) {
        int buddy = start ^ size;
        int pos = dlist_locate(&b->free_list[order_of(size)], buddy);
        if (pos < 1) {
            return insert_sorted(&b->free_list[order_of(size)], start) ? 0 : -1;
        }
        if (!dlist_delete(&b->free_list[order_of(size)], pos, NULL)) {
            return -1;
        }
        if (buddy < start) {
            start = buddy;
        }
        size *= 2;
        (*merges)++;
    }
}

static void sort_blocks(KBuddyBlk *blk, int n) {
    int i;
    for (i = 1; i < n; i++) {
        KBuddyBlk key = blk[i];
        int j = i;
        while (j > 0 && blk[j - 1].start > key.start) {
            blk[j] = blk[j - 1];
            j--;
        }
        blk[j] = key;
    }
}

int k_buddy_list(const KBuddy *b, int want_free, KBuddyBlk *out, int cap) {
    KBuddyBlk tmp[K_BUDDY_JOBS];
    int n = 0;
    int i;
    int copy;
    if (b == NULL || !b->ready || out == NULL || cap < 0) {
        return -1;
    }
    if (want_free) {
        int order;
        for (order = 0; order < b->norder; order++) {
            int len = dlist_length(&b->free_list[order]);
            for (i = 1; i <= len && n < K_BUDDY_JOBS; i++) {
                int start = 0;
                if (!dlist_get(&b->free_list[order], i, &start)) {
                    return -1;
                }
                tmp[n].start = start;
                tmp[n].size = 1 << order;
                tmp[n].name[0] = '\0';
                n++;
            }
        }
    } else {
        for (i = 0; i < b->njob && n < K_BUDDY_JOBS; i++) {
            if (!b->job[i].used) {
                continue;
            }
            tmp[n].start = b->job[i].start;
            tmp[n].size = b->job[i].size;
            copy_name(tmp[n].name, b->job[i].name);
            n++;
        }
    }
    sort_blocks(tmp, n);
    copy = n < cap ? n : cap;
    for (i = 0; i < copy; i++) {
        out[i] = tmp[i];
    }
    return n;
}

int k_buddy_links(const KBuddy *b) {
    int order;
    if (b == NULL || !b->ready) {
        return 0;
    }
    for (order = 0; order < b->norder; order++) {
        int size = 1 << order;
        int n = dlist_length(&b->free_list[order]);
        int i;
        if (!dlist_links_ok(&b->free_list[order])) {
            return 0;
        }
        for (i = 1; i <= n; i++) {
            int start = 0;
            if (!dlist_get(&b->free_list[order], i, &start) || start < 0 || start >= b->cap ||
                (start & (size - 1)) != 0) {
                return 0;
            }
        }
    }
    return 1;
}
