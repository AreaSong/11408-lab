#include "blk.h"

#include "asl.h"
#include "bin.h"

#include <string.h>

int blk_mode(const char *name) {
    if (strcmp(name, "seq") == 0) {
        return 0;
    }
    if (strcmp(name, "bin") == 0) {
        return 1;
    }
    return -1;
}

void blk_begin(BlkTable *t, int index_bin, int inner_bin) {
    memset(t, 0, sizeof *t);
    t->index_bin = index_bin;
    t->inner_bin = inner_bin;
}

static int key_ok(int key) {
    return key >= KEY_LO && key <= KEY_HI;
}

static void span_of(const int *keys, int n, int *mn, int *mx) {
    int i;
    *mn = keys[0];
    *mx = keys[0];
    for (i = 1; i < n; i++) {
        if (keys[i] < *mn) {
            *mn = keys[i];
        }
        if (keys[i] > *mx) {
            *mx = keys[i];
        }
    }
}

static int rising(const int *keys, int n) {
    int i;
    for (i = 1; i < n; i++) {
        if (keys[i] <= keys[i - 1]) {
            return 0;
        }
    }
    return 1;
}

int blk_add(BlkTable *t, const int *keys, int n) {
    int i;
    int mn;
    int mx;
    int id;
    if (n < 1 || n > BLOCK_LEN) {
        return S_RANGE;
    }
    if (t->b >= BLOCK_MAX) {
        return S_FULL;
    }
    for (i = 0; i < n; i++) {
        if (!key_ok(keys[i])) {
            return S_RANGE;
        }
    }
    /* 块内折半和整表折半一样，要求这一块严格升序。顺序查找不要求。 */
    if (t->inner_bin && !rising(keys, n)) {
        return S_ORDER;
    }
    span_of(keys, n, &mn, &mx);
    if (t->b > 0 && !(t->maxk[t->b] < mn)) {
        return S_ORDER;
    }
    id = t->b + 1;
    t->cnt[id] = n;
    t->mink[id] = mn;
    t->maxk[id] = mx;
    for (i = 1; i <= n; i++) {
        t->key[id][i] = keys[i - 1];
    }
    t->b = id;
    return S_OK;
}

void blk_show(const BlkTable *t, FILE *out) {
    int i;
    int j;
    int n = 0;
    for (i = 1; i <= t->b; i++) {
        n += t->cnt[i];
    }
    fprintf(out, "show b=%d n=%d index=%s inner=%s\n", t->b, n, t->index_bin ? "bin" : "seq",
            t->inner_bin ? "bin" : "seq");
    for (i = 1; i <= t->b; i++) {
        fprintf(out, "block %d n=%d min=%d max=%d", i, t->cnt[i], t->mink[i], t->maxk[i]);
        for (j = 1; j <= t->cnt[i]; j++) {
            fprintf(out, " %d", t->key[i][j]);
        }
        fprintf(out, "\n");
    }
}

static int locate_seq(const BlkTable *t, int key, FILE *out, int *cmp) {
    int i;
    *cmp = 0;
    for (i = 1; i <= t->b; i++) {
        (*cmp)++;
        if (key > t->maxk[i]) {
            if (out != NULL) {
                fprintf(out, "index i=%d max=%d >\n", i, t->maxk[i]);
            }
            continue;
        }
        if (out != NULL) {
            fprintf(out, "index i=%d max=%d %c\n", i, t->maxk[i], key == t->maxk[i] ? '=' : '<');
        }
        return i;
    }
    return 0;
}

/* 在最大关键字上折半。相等就落在这一块；失败时 low 是第一个更大的块。 */
static int locate_bin(const BlkTable *t, int key, FILE *out, int *cmp) {
    int low = 1;
    int high = t->b;
    *cmp = 0;
    while (low <= high) {
        int mid = (low + high) / 2;
        int mx = t->maxk[mid];
        (*cmp)++;
        if (key == mx) {
            if (out != NULL) {
                fprintf(out, "index low=%d high=%d mid=%d max=%d =\n", low, high, mid, mx);
            }
            return mid;
        }
        if (out != NULL) {
            fprintf(out, "index low=%d high=%d mid=%d max=%d %c\n", low, high, mid, mx, key < mx ? '<' : '>');
        }
        if (key < mx) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    if (out != NULL) {
        fprintf(out, "index low=%d high=%d stop\n", low, high);
    }
    if (low >= 1 && low <= t->b) {
        return low;
    }
    return 0;
}

static int locate(const BlkTable *t, int key, FILE *out, int *cmp) {
    if (t->index_bin) {
        return locate_bin(t, key, out, cmp);
    }
    return locate_seq(t, key, out, cmp);
}

static int inner_seq(const BlkTable *t, int block, int key, FILE *out, int *cmp) {
    int j;
    *cmp = 0;
    for (j = 1; j <= t->cnt[block]; j++) {
        int a = t->key[block][j];
        (*cmp)++;
        if (a == key) {
            if (out != NULL) {
                fprintf(out, "inner j=%d a=%d =\n", j, a);
            }
            return j;
        }
        if (out != NULL) {
            fprintf(out, "inner j=%d a=%d !=\n", j, a);
        }
    }
    return 0;
}

static int inner_bin(const BlkTable *t, int block, int key, FILE *out, int *cmp) {
    int low = 1;
    int high = t->cnt[block];
    *cmp = 0;
    while (low <= high) {
        int mid = (low + high) / 2;
        int a = t->key[block][mid];
        (*cmp)++;
        if (key == a) {
            if (out != NULL) {
                fprintf(out, "inner low=%d high=%d mid=%d a=%d =\n", low, high, mid, a);
            }
            return mid;
        }
        if (out != NULL) {
            fprintf(out, "inner low=%d high=%d mid=%d a=%d %c\n", low, high, mid, a, key < a ? '<' : '>');
        }
        if (key < a) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    if (out != NULL) {
        fprintf(out, "inner low=%d high=%d stop\n", low, high);
    }
    return 0;
}

static int search_inner(const BlkTable *t, int block, int key, FILE *out, int *cmp) {
    if (t->inner_bin) {
        return inner_bin(t, block, key, out, cmp);
    }
    return inner_seq(t, block, key, out, cmp);
}

int blk_find(const BlkTable *t, int key, FILE *out, int *cmp) {
    int ic = 0;
    int cc = 0;
    int block;
    int pos;
    block = locate(t, key, out, &ic);
    if (block == 0) {
        if (out != NULL) {
            fprintf(out, "miss cmp=%d\n", ic);
        }
        *cmp = ic;
        return 0;
    }
    if (out != NULL) {
        fprintf(out, "block=%d\n", block);
    }
    pos = search_inner(t, block, key, out, &cc);
    *cmp = ic + cc;
    if (out == NULL) {
        return pos;
    }
    if (pos > 0) {
        fprintf(out, "hit j=%d cmp=%d\n", pos, *cmp);
    } else {
        fprintf(out, "miss cmp=%d\n", *cmp);
    }
    return pos;
}

static int uniform_size(const BlkTable *t) {
    int s;
    int i;
    if (t->b < 1) {
        return 0;
    }
    s = t->cnt[1];
    for (i = 2; i <= t->b; i++) {
        if (t->cnt[i] != s) {
            return 0;
        }
    }
    return s;
}

int blk_formula(int index_bin, int inner_bin, int blocks, int size, BlkFrac *out) {
    long long index_n;
    long long index_d;
    long long inner_n;
    long long inner_d;
    long long sum = 0;
    if (blocks < 1 || size < 1 || blocks > TREE_MAX || size > TREE_MAX) {
        return S_RANGE;
    }
    if (!index_bin) {
        index_n = (long long)blocks + 1;
        index_d = 2;
    } else if (bin_tree_sum(blocks, 0, &sum, NULL) != S_OK) {
        return S_FULL;
    } else {
        index_n = sum;
        index_d = blocks;
    }
    if (!inner_bin) {
        inner_n = (long long)size + 1;
        inner_d = 2;
    } else if (bin_tree_sum(size, 0, &sum, NULL) != S_OK) {
        return S_FULL;
    } else {
        inner_n = sum;
        inner_d = size;
    }
    /* ASL = 索引 ASL + 块内 ASL，通分后仍是一个分数。 */
    out->num = index_n * inner_d + inner_n * index_d;
    out->den = index_d * inner_d;
    return S_OK;
}

static void add_key(const BlkTable *t, int block, int j, long long *index_n, long long *inner_n) {
    int ic = 0;
    int cc = 0;
    int where = locate(t, t->key[block][j], NULL, &ic);
    if (where != 0) {
        search_inner(t, where, t->key[block][j], NULL, &cc);
    }
    *index_n += ic;
    *inner_n += cc;
}

void blk_measure(const BlkTable *t, BlkAsl *a) {
    long long index_n = 0;
    long long inner_n = 0;
    int total = 0;
    int s;
    int i;
    int j;
    memset(a, 0, sizeof *a);
    for (i = 1; i <= t->b; i++) {
        for (j = 1; j <= t->cnt[i]; j++) {
            add_key(t, i, j, &index_n, &inner_n);
            total++;
        }
    }
    a->index_n = index_n;
    a->index_d = total;
    a->inner_n = inner_n;
    a->inner_d = total;
    a->exact_n = index_n + inner_n;
    a->exact_d = total;
    s = uniform_size(t);
    if (s > 0) {
        BlkFrac f;
        if (blk_formula(t->index_bin, t->inner_bin, t->b, s, &f) == S_OK) {
            a->rough_n = f.num;
            a->rough_d = f.den;
        }
    }
}

void blk_print_asl(const BlkTable *t, FILE *out) {
    BlkAsl a;
    blk_measure(t, &a);
    fprintf(out, "asl exact=");
    asl_write(out, a.exact_n, a.exact_d);
    fprintf(out, "\nparts index=");
    asl_write(out, a.index_n, a.index_d);
    fprintf(out, " inner=");
    asl_write(out, a.inner_n, a.inner_d);
    fprintf(out, "\n");
    if (a.rough_d == 0) {
        fprintf(out, "rough=- match=-\n");
        return;
    }
    fprintf(out, "rough=");
    asl_write(out, a.rough_n, a.rough_d);
    fprintf(out, " match=%s\n", asl_same(a.exact_n, a.exact_d, a.rough_n, a.rough_d) ? "yes" : "no");
}

static int fill_best(const BestAsk *ask, long long *nums, long long *dens, int *blocks) {
    int i;
    if (ask->ns < 1 || ask->ns > BEST_NS || ask->n < 1 || ask->n > TREE_MAX) {
        return S_RANGE;
    }
    for (i = 0; i < ask->ns; i++) {
        BlkFrac f;
        int s = ask->sizes[i];
        int rc;
        if (s < 1 || s > ask->n) {
            return S_RANGE;
        }
        if (ask->n % s != 0) {
            return S_DIV;
        }
        rc = blk_formula(ask->index_bin, ask->inner_bin, ask->n / s, s, &f);
        if (rc != S_OK) {
            return rc;
        }
        nums[i] = f.num;
        dens[i] = f.den;
        blocks[i] = ask->n / s;
    }
    return S_OK;
}

static void print_pick(FILE *out, int pick, long long num, long long den, const int *tie, int nt) {
    int i;
    fprintf(out, "pick s=%d asl=", pick);
    asl_write(out, num, den);
    fprintf(out, " tie=");
    if (nt == 0) {
        fprintf(out, "-\n");
        return;
    }
    for (i = 0; i < nt; i++) {
        fprintf(out, "%s%d", i == 0 ? "" : " ", tie[i]);
    }
    fprintf(out, "\n");
}

int blk_best(const BestAsk *ask, FILE *out) {
    long long nums[BEST_NS];
    long long dens[BEST_NS];
    int blocks[BEST_NS];
    int tie[BEST_NS];
    int nt = 0;
    int pick = 0;
    int rc;
    int i;
    long long pnum = 0;
    long long pden = 1;
    rc = fill_best(ask, nums, dens, blocks);
    if (rc != S_OK) {
        return rc;
    }
    fprintf(out, "best n=%d index=%s inner=%s\n", ask->n, ask->index_bin ? "bin" : "seq",
            ask->inner_bin ? "bin" : "seq");
    for (i = 0; i < ask->ns; i++) {
        fprintf(out, "s=%d b=%d asl=", ask->sizes[i], blocks[i]);
        asl_write(out, nums[i], dens[i]);
        fprintf(out, "\n");
        if (i == 0 || asl_less(nums[i], dens[i], pnum, pden)) {
            pick = ask->sizes[i];
            pnum = nums[i];
            pden = dens[i];
            nt = 0;
        } else if (asl_same(nums[i], dens[i], pnum, pden)) {
            tie[nt++] = ask->sizes[i];
        }
    }
    print_pick(out, pick, pnum, pden, tie, nt);
    return S_OK;
}
