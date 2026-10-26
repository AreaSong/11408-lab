#include "asl.h"

#include <stdio.h>

static int in_range(int n) {
    return n >= 1 && n <= ASL_N_MAX;
}

/* 第 index 组（从 0 起）的最大关键字。记录关键字是 1..n，按组切。 */
static int group_end(int n, int groups, int index) {
    int base = n / groups;
    int extra = n % groups;
    int before;
    int size;

    if (index < extra) {
        before = index * (base + 1);
        size = base + 1;
    } else {
        before = extra * (base + 1) + (index - extra) * base;
        size = base;
    }
    return before + size;
}

static int group_of(int n, int groups, int key) {
    int i;

    for (i = 0; i < groups; i++) {
        if (key <= group_end(n, groups, i)) {
            return i;
        }
    }
    return -1;
}

static int group_start(int n, int groups, int index) {
    if (index == 0) {
        return 1;
    }
    return group_end(n, groups, index - 1) + 1;
}

/* 折半：下标 1..n，中间值就是下标。low、high 闭区间，mid 向下取整。 */
static int bin_key(int n, int key, AslWalk *w) {
    int low = 1;
    int high = n;
    int cmp = 0;

    while (low <= high) {
        int mid = (low + high) / 2;
        cmp++;
        if (w != 0 && w->nstep < ASL_STEPS) {
            snprintf(w->step[w->nstep], ASL_STEP, "step mid=%d", mid);
            w->nstep++;
        }
        if (mid == key) {
            return cmp;
        }
        if (key < mid) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return -1;
}

/* 在各组最大关键字上做精确折半，得到「这一组」要比较几次。 */
static int bin_group(int n, int groups, int target, AslWalk *w) {
    int low = 1;
    int high = groups;
    int cmp = 0;

    while (low <= high) {
        int mid = (low + high) / 2;
        int value = group_end(n, groups, mid - 1);
        cmp++;
        if (w != 0 && w->nstep < ASL_STEPS) {
            snprintf(w->step[w->nstep], ASL_STEP, "step index mid=%d max=%d", mid, value);
            w->nstep++;
        }
        if (value == target) {
            return cmp;
        }
        if (target < value) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return -1;
}

static void add_group_steps(int start, int key, AslWalk *w) {
    int k;

    for (k = start; k <= key; k++) {
        if (w->nstep < ASL_STEPS) {
            snprintf(w->step[w->nstep], ASL_STEP, "step group key=%d", k);
            w->nstep++;
        }
    }
}

int asl_seq(int n, Frac *out) {
    if (!in_range(n)) {
        return -1;
    }
    /* 等概率成功查找：第 k 条比 k 次，平均 (n+1)/2。 */
    frac_set(out, (long long)n + 1, 2);
    return 0;
}

int asl_index(int n, Frac *out) {
    long long sum = 0;
    int key;

    if (!in_range(n)) {
        return -1;
    }
    for (key = 1; key <= n; key++) {
        sum += bin_key(n, key, 0);
    }
    frac_set(out, sum, n);
    return 0;
}

static int isam_one(int n, int groups, int binary, int key, AslWalk *w) {
    int gi = group_of(n, groups, key);
    int start = group_start(n, groups, gi);
    int index_cmp;
    int i;

    if (!binary) {
        index_cmp = gi + 1;
        if (w != 0) {
            for (i = 0; i <= gi; i++) {
                if (w->nstep < ASL_STEPS) {
                    snprintf(w->step[w->nstep], ASL_STEP, "step index max=%d",
                             group_end(n, groups, i));
                    w->nstep++;
                }
            }
        }
    } else {
        index_cmp = bin_group(n, groups, group_end(n, groups, gi), w);
    }
    if (w != 0) {
        add_group_steps(start, key, w);
    }
    return index_cmp + (key - start + 1);
}

int asl_isam(int n, int groups, int binary, Frac *out) {
    long long sum = 0;
    int key;

    if (!in_range(n) || groups < 1 || groups > n) {
        return -1;
    }
    for (key = 1; key <= n; key++) {
        sum += isam_one(n, groups, binary, key, 0);
    }
    frac_set(out, sum, n);
    return 0;
}

int asl_walk(int org, int n, int groups, int key, AslWalk *out) {
    int cmp;

    if (out == 0 || key < 1 || key > n) {
        return -1;
    }
    out->cmp = 0;
    out->nstep = 0;
    if (org == ASL_SEQ) {
        if (!in_range(n)) {
            return -1;
        }
        cmp = key;
    } else if (org == ASL_INDEX) {
        if (!in_range(n)) {
            return -1;
        }
        cmp = bin_key(n, key, 0);
    } else if (org == ASL_ISAM_SEQ || org == ASL_ISAM_BIN) {
        if (!in_range(n) || groups < 1 || groups > n) {
            return -1;
        }
        cmp = isam_one(n, groups, org == ASL_ISAM_BIN, key, 0);
    } else {
        return -1;
    }
    if (cmp < 0 || cmp > ASL_STEPS) {
        return -2;
    }
    out->nstep = 0;
    if (org == ASL_SEQ) {
        int i;
        for (i = 1; i <= key; i++) {
            snprintf(out->step[out->nstep], ASL_STEP, "step key=%d", i);
            out->nstep++;
        }
    } else if (org == ASL_INDEX) {
        bin_key(n, key, out);
    } else {
        isam_one(n, groups, org == ASL_ISAM_BIN, key, out);
    }
    out->cmp = cmp;
    return 0;
}

int asl_sizes(int n, int groups, char *buf, int cap) {
    int base;
    int extra;
    int i;
    int pos = 0;

    if (!in_range(n) || groups < 1 || groups > n || cap < 2) {
        return -1;
    }
    base = n / groups;
    extra = n % groups;
    if (extra == 0) {
        buf[0] = '\0';
        return 0;
    }
    for (i = 0; i < groups; i++) {
        int size = base + (i < extra ? 1 : 0);
        int wrote = snprintf(buf + pos, (size_t)(cap - pos), "%s%d", i ? "+" : "", size);
        if (wrote < 0 || wrote >= cap - pos) {
            return -1;
        }
        pos += wrote;
    }
    return 1;
}
