#include "group.h"

#include <string.h>

void group_init(Group *g, int cap) {
    if (g == 0) {
        return;
    }
    memset(g, 0, sizeof *g);
    if (cap < 1) {
        cap = 1;
    }
    if (cap > GROUP_CAP) {
        cap = GROUP_CAP;
    }
    g->cap = cap;
}

int group_fill(Group *g, int block, int count, int first, int step) {
    int i;
    int value = first;

    if (g == 0 || block < 0 || block >= GROUP_DISK || count < 1 || count > g->cap || step == 0) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        long long next;
        if (value < 0 || value >= GROUP_DISK) {
            return -1;
        }
        g->db[block][i] = value;
        if (i + 1 == count) {
            break;
        }
        next = (long long)value + step;
        if (next < 0 || next >= GROUP_DISK) {
            return -1;
        }
        value = (int)next;
    }
    g->dn[block] = (unsigned char)count;
    g->used[block] = 1;
    return 0;
}

int group_set_stack(Group *g, const int *blk, int n) {
    int i;

    if (g == 0 || n < 0 || n > g->cap || (n > 0 && blk == 0)) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (blk[i] < 0 || blk[i] >= GROUP_DISK) {
            return -1;
        }
    }
    g->n = n;
    for (i = 0; i < n; i++) {
        g->blk[i] = blk[i];
    }
    return 0;
}

int group_total(const Group *g) {
    int blk[GROUP_CAP];
    int n;
    int sum = 0;
    int seen[64];
    int ns = 0;
    int i;

    if (g == 0) {
        return -1;
    }
    n = g->n;
    for (i = 0; i < n; i++) {
        blk[i] = g->blk[i];
    }
    while (n > 0 && ns < 64) {
        int bottom = blk[0];
        int next_n;
        sum += n;
        if (bottom < 0 || bottom >= GROUP_DISK || !g->used[bottom]) {
            break;
        }
        for (i = 0; i < ns; i++) {
            if (seen[i] == bottom) {
                return sum;
            }
        }
        seen[ns++] = bottom;
        next_n = g->dn[bottom];
        if (next_n > g->cap) {
            return -1;
        }
        for (i = 0; i < next_n; i++) {
            blk[i] = g->db[bottom][i];
        }
        n = next_n;
    }
    return sum;
}

int group_alloc(Group *g, int *block, int *did_read) {
    int head;
    int n;
    int i;

    if (g == 0 || block == 0 || did_read == 0) {
        return -1;
    }
    if (g->n <= 0) {
        return -2;
    }
    head = g->blk[0];
    if (g->n == 1 && head >= 0 && head < GROUP_DISK && g->used[head]) {
        n = g->dn[head];
        if (n < 0 || n > g->cap) {
            return -1;
        }
        for (i = 0; i < n; i++) {
            g->blk[i] = g->db[head][i];
        }
        g->n = n;
        g->used[head] = 0;
        *block = head;
        *did_read = 1;
        return 0;
    }
    g->n--;
    *block = g->blk[g->n];
    if (*block >= 0 && *block < GROUP_DISK) {
        g->used[*block] = 0;
    }
    *did_read = 0;
    return 0;
}

int group_free(Group *g, int block, int *did_write) {
    int i;

    if (g == 0 || did_write == 0 || block < 0 || block >= GROUP_DISK) {
        return -1;
    }
    if (g->n < g->cap) {
        g->blk[g->n++] = block;
        *did_write = 0;
        return 0;
    }
    /* 栈已满：把这一组写进刚回收的块，它成为新的组长块，栈里只留它。 */
    g->dn[block] = (unsigned char)g->n;
    for (i = 0; i < g->n; i++) {
        g->db[block][i] = g->blk[i];
    }
    g->used[block] = 1;
    g->n = 1;
    g->blk[0] = block;
    *did_write = 1;
    return 0;
}
