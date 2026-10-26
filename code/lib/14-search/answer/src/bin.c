#include "bin.h"

#include <stdlib.h>
#include <string.h>

int bin_mode(const char *name) {
    if (strcmp(name, "floor") == 0) {
        return 0;
    }
    if (strcmp(name, "ceil") == 0) {
        return 1;
    }
    return -1;
}

static int mid_of(int low, int high, int ceil_mid) {
    if (ceil_mid) {
        return (low + high + 1) / 2;
    }
    return (low + high) / 2;
}

/* 区间 [low, high] 的中点做根。空区间返回 0，调用方把它当成外部结点。 */
static int build(int ceil_mid, int low, int high, int depth, int *left, int *right, int *dep, int *height) {
    int mid;
    if (low > high) {
        return 0;
    }
    mid = mid_of(low, high, ceil_mid);
    dep[mid] = depth;
    if (depth > *height) {
        *height = depth;
    }
    left[mid] = build(ceil_mid, low, mid - 1, depth + 1, left, right, dep, height);
    right[mid] = build(ceil_mid, mid + 1, high, depth + 1, left, right, dep, height);
    return mid;
}

static int alloc_tree(int n, int **left, int **right, int **dep) {
    *left = calloc((size_t)n + 1, sizeof(int));
    *right = calloc((size_t)n + 1, sizeof(int));
    *dep = calloc((size_t)n + 1, sizeof(int));
    if (*left == NULL || *right == NULL || *dep == NULL) {
        free(*left);
        free(*right);
        free(*dep);
        *left = NULL;
        *right = NULL;
        *dep = NULL;
        return 0;
    }
    return 1;
}

int bin_bound(int n) {
    int h = 0;
    int p = 1;
    if (n < 1) {
        return 0;
    }
    /* floor(log2 n)+1，也就是最小的 h 使 2^h - 1 >= n 不成立时再加一层。 */
    while (p <= n / 2) {
        p *= 2;
        h++;
    }
    return h + 1;
}

int bin_tree_sum(int n, int ceil_mid, long long *sum, int *height) {
    int *left;
    int *right;
    int *dep;
    int h = 0;
    int i;
    long long total = 0;
    if (n < 1 || n > TREE_MAX || (ceil_mid != 0 && ceil_mid != 1)) {
        return S_RANGE;
    }
    if (!alloc_tree(n, &left, &right, &dep)) {
        return S_FULL;
    }
    build(ceil_mid, 1, n, 1, left, right, dep, &h);
    for (i = 1; i <= n; i++) {
        total += dep[i];
    }
    free(left);
    free(right);
    free(dep);
    if (sum != NULL) {
        *sum = total;
    }
    if (height != NULL) {
        *height = h;
    }
    return S_OK;
}

int bin_fill_depth(int n, int ceil_mid, int *depth) {
    int *left;
    int *right;
    int *dep;
    int h = 0;
    int i;
    if (depth == NULL || n < 1 || n > TREE_MAX || (ceil_mid != 0 && ceil_mid != 1)) {
        return S_RANGE;
    }
    if (!alloc_tree(n, &left, &right, &dep)) {
        return S_FULL;
    }
    build(ceil_mid, 1, n, 1, left, right, dep, &h);
    depth[0] = 0;
    for (i = 1; i <= n; i++) {
        depth[i] = dep[i];
    }
    free(left);
    free(right);
    free(dep);
    return S_OK;
}

static int key_ok(int key) {
    return key >= KEY_LO && key <= KEY_HI;
}

int bin_load(BinTable *t, int ceil_mid, const int *keys, int n) {
    int i;
    int height = 0;
    if ((ceil_mid != 0 && ceil_mid != 1) || n < 1 || n > TABLE_MAX) {
        return S_RANGE;
    }
    for (i = 0; i < n; i++) {
        if (!key_ok(keys[i])) {
            return S_RANGE;
        }
        if (i > 0 && keys[i] <= keys[i - 1]) {
            return S_ORDER;
        }
    }
    memset(t, 0, sizeof *t);
    t->n = n;
    t->ceil_mid = ceil_mid;
    for (i = 1; i <= n; i++) {
        t->key[i] = keys[i - 1];
        t->weight[i] = 1;
    }
    t->root = build(ceil_mid, 1, n, 1, t->left, t->right, t->depth, &height);
    t->height = height;
    return S_OK;
}

int bin_weight(BinTable *t, const int *w, int n) {
    int i;
    if (n != t->n) {
        return S_RANGE;
    }
    for (i = 0; i < n; i++) {
        if (w[i] < 1) {
            return S_RANGE;
        }
    }
    for (i = 1; i <= n; i++) {
        t->weight[i] = w[i - 1];
    }
    t->weighted = 1;
    return S_OK;
}

/* 每个空孩子是一个失败入口，比较次数等于父结点的层数。n 个结点恰有 n+1 个空孩子。 */
static int collect_fail(const BinTable *t, int node, int *gap, int ng, long long *sum) {
    if (node == 0) {
        return ng;
    }
    if (t->left[node] == 0) {
        gap[ng++] = t->depth[node];
        *sum += t->depth[node];
    } else {
        ng = collect_fail(t, t->left[node], gap, ng, sum);
    }
    if (t->right[node] == 0) {
        gap[ng++] = t->depth[node];
        *sum += t->depth[node];
    } else {
        ng = collect_fail(t, t->right[node], gap, ng, sum);
    }
    return ng;
}

void bin_asl(const BinTable *t, AslPair *a) {
    int gap[TABLE_MAX + 2];
    int ng;
    long long fail_n = 0;
    int i;
    a->succ_n = 0;
    a->succ_d = 0;
    for (i = 1; i <= t->n; i++) {
        long long w = t->weighted ? t->weight[i] : 1;
        a->succ_n += w * t->depth[i];
        a->succ_d += w;
    }
    ng = collect_fail(t, t->root, gap, 0, &fail_n);
    a->fail_n = fail_n;
    a->fail_d = ng;
}

void bin_print_asl(const BinTable *t, FILE *out) {
    AslPair a;
    bin_asl(t, &a);
    fprintf(out, "asl succ=");
    asl_write(out, a.succ_n, a.succ_d);
    fprintf(out, " fail=");
    asl_write(out, a.fail_n, a.fail_d);
    fprintf(out, " weight=%s\n", t->weighted ? "yes" : "no");
}

void bin_print_tree(const BinTable *t, FILE *out) {
    int gap[TABLE_MAX + 2];
    long long ignore = 0;
    int ng;
    int i;
    fprintf(out, "root=%d\n", t->root);
    fprintf(out, "depth");
    for (i = 1; i <= t->n; i++) {
        fprintf(out, " %d", t->depth[i]);
    }
    fprintf(out, "\n");
    for (i = 1; i <= t->n; i++) {
        fprintf(out, "node i=%d key=%d depth=%d left=%d right=%d\n", i, t->key[i], t->depth[i], t->left[i],
                t->right[i]);
    }
    ng = collect_fail(t, t->root, gap, 0, &ignore);
    fprintf(out, "fail");
    for (i = 0; i < ng; i++) {
        fprintf(out, " %d", gap[i]);
    }
    fprintf(out, "\nheight=%d\n", t->height);
}

void bin_report(const BinTable *t, FILE *out) {
    int i;
    fprintf(out, "bin %s n=%d\n", t->ceil_mid ? "ceil" : "floor", t->n);
    fprintf(out, "key");
    for (i = 1; i <= t->n; i++) {
        fprintf(out, " %d", t->key[i]);
    }
    fprintf(out, "\n");
    bin_print_tree(t, out);
    bin_print_asl(t, out);
}

int bin_find(const BinTable *t, int key, FILE *out, int *cmp) {
    int low = 1;
    int high = t->n;
    int ncmp = 0;
    while (low <= high) {
        int mid = mid_of(low, high, t->ceil_mid);
        int a = t->key[mid];
        ncmp++;
        if (key == a) {
            if (out != NULL) {
                fprintf(out, "low=%d high=%d mid=%d a=%d =\n", low, high, mid, a);
                fprintf(out, "hit i=%d cmp=%d\n", mid, ncmp);
            }
            *cmp = ncmp;
            return mid;
        }
        if (out != NULL) {
            fprintf(out, "low=%d high=%d mid=%d a=%d %c\n", low, high, mid, a, key < a ? '<' : '>');
        }
        if (key < a) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    if (out != NULL) {
        fprintf(out, "low=%d high=%d stop\n", low, high);
        fprintf(out, "miss cmp=%d\n", ncmp);
    }
    *cmp = ncmp;
    return 0;
}

static void print_open(FILE *out, int has_lo, int lo, int has_hi, int hi) {
    fputc('(', out);
    if (has_lo) {
        fprintf(out, "%d", lo);
    } else {
        fprintf(out, "-inf");
    }
    fputc(',', out);
    if (has_hi) {
        fprintf(out, "%d", hi);
    } else {
        fprintf(out, "+inf");
    }
    fputc(')', out);
}

static int in_open(int key, int has_lo, int lo, int has_hi, int hi) {
    if (has_lo && key <= lo) {
        return 0;
    }
    if (has_hi && key >= hi) {
        return 0;
    }
    return 1;
}

int bin_path(const int *keys, int n, FILE *out) {
    int has_lo = 0;
    int has_hi = 0;
    int lo = 0;
    int hi = 0;
    int i;
    if (n < 1) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        int ok = in_open(keys[i], has_lo, lo, has_hi, hi);
        fprintf(out, "%d %d ", i + 1, keys[i]);
        print_open(out, has_lo, lo, has_hi, hi);
        fprintf(out, " %s\n", ok ? "in" : "out");
        if (!ok) {
            fprintf(out, "no\n");
            return 0;
        }
        if (i + 1 >= n) {
            break;
        }
        /* 下一个比当前小，说明这次往左，之后的关键字都得小于当前。 */
        if (keys[i + 1] < keys[i]) {
            has_hi = 1;
            hi = keys[i];
        } else if (keys[i + 1] > keys[i]) {
            has_lo = 1;
            lo = keys[i];
        } else {
            has_lo = 1;
            has_hi = 1;
            lo = keys[i];
            hi = keys[i];
        }
    }
    fprintf(out, "yes\n");
    return 1;
}

int bin_suitable(const char *store, const char *ord) {
    int known_store;
    int sorted;
    int random_ok;
    known_store = strcmp(store, "array") == 0 || strcmp(store, "list") == 0 || strcmp(store, "static") == 0;
    if (!known_store || (strcmp(ord, "order") != 0 && strcmp(ord, "chaos") != 0)) {
        return -1;
    }
    sorted = strcmp(ord, "order") == 0;
    /* 静态链表存在数组里，但逻辑顺序靠游标，下标中点不是逻辑中点。 */
    random_ok = strcmp(store, "array") == 0;
    return sorted && random_ok;
}

void bin_fit(const char *store, const char *ord, FILE *out) {
    int ok = bin_suitable(store, ord);
    int sorted = strcmp(ord, "order") == 0;
    int random_ok = strcmp(store, "array") == 0;
    fprintf(out, "fit %s %s", store, ord);
    if (ok == 1) {
        fprintf(out, " yes\n");
        return;
    }
    fprintf(out, " no");
    if (!sorted) {
        fprintf(out, " sorted");
    }
    if (!random_ok) {
        fprintf(out, " random");
    }
    fprintf(out, "\n");
}

int bin_worst(int n, int ceil_mid, FILE *out) {
    int height = 0;
    int rc = bin_tree_sum(n, ceil_mid, NULL, &height);
    if (rc != S_OK) {
        return rc;
    }
    fprintf(out, "worst %s n=%d height=%d bound=%d\n", ceil_mid ? "ceil" : "floor", n, height, bin_bound(n));
    return S_OK;
}
