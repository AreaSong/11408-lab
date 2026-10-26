#include "hash.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

long long hash_gcd(long long a, long long b) {
    if (a < 0) {
        a = -a;
    }
    if (b < 0) {
        b = -b;
    }
    while (b != 0) {
        long long r = a % b;
        a = b;
        b = r;
    }
    return a == 0 ? 1 : a;
}

void hash_print_frac(const char *name, long long num, long long den) {
    long long g = hash_gcd(num, den);
    printf("%s=%lld/%lld\n", name, num / g, den / g);
}

static int bucket_len(const HashTab *t, int h) {
    int n = 0;
    HashNode *p;
    for (p = t->head[h]; p != NULL; p = p->next) {
        n++;
    }
    return n;
}

/* 开放定址：从散列地址走到空位，空位那一次也算。删除标记继续走。 */
static int open_fail_at(const HashTab *t, int h0) {
    int seq[HASH_M_MAX];
    int len = hash_probe(t, h0, 1, seq);
    int i;
    for (i = 0; i < len; i++) {
        if (t->state[seq[i]] == CELL_EMPTY) {
            return i + 1;
        }
    }
    return len;
}

static void succ_open(const HashTab *t, HashAsl *out) {
    long long sum = 0;
    int i;
    if (t->n == 0) {
        return;
    }
    for (i = 0; i < t->m; i++) {
        OpenWalk w;
        if (t->state[i] != CELL_FULL) {
            continue;
        }
        open_find(t, t->key[i], &w);
        sum += w.cmp;
    }
    out->has_succ = 1;
    out->succ_num = sum;
    out->succ_den = t->n;
}

static void succ_chain(const HashTab *t, HashAsl *out) {
    long long sum = 0;
    int i;
    if (t->n == 0) {
        return;
    }
    for (i = 0; i < t->m; i++) {
        int c = 1;
        HashNode *p;
        for (p = t->head[i]; p != NULL; p = p->next, c++) {
            sum += c;
        }
    }
    out->has_succ = 1;
    out->succ_num = sum;
    out->succ_den = t->n;
}

static void fail_chain(const HashTab *t, int count_null, HashAsl *out) {
    long long sum = 0;
    int h;
    for (h = 0; h < t->p; h++) {
        sum += bucket_len(t, h);
        if (count_null) {
            sum++;
        }
    }
    out->fail_num = sum;
    out->fail_den = t->p;
}

void hash_asl(const HashTab *t, int count_null, HashAsl *out) {
    long long sum = 0;
    int h;
    memset(out, 0, sizeof *out);
    if (t->kind == KIND_CHAIN) {
        succ_chain(t, out);
        fail_chain(t, count_null, out);
        return;
    }
    succ_open(t, out);
    /* 双散列的下一步依赖 H2(key)，失败查找不能只按散列地址平均。 */
    if (t->kind == KIND_DOUBLE) {
        out->depend = 1;
        return;
    }
    for (h = 0; h < t->p; h++) {
        sum += open_fail_at(t, h);
    }
    out->fail_num = sum;
    out->fail_den = t->p;
}

static int formula_linear(long long n, long long m, HashFormula *out) {
    long long gap;
    if (n >= m) {
        return 0;
    }
    gap = m - n;
    out->succ_exact = 1;
    out->succ_num = 2 * m - n;
    out->succ_den = 2 * gap;
    out->fail_exact = 1;
    out->fail_num = gap * gap + m * m;
    out->fail_den = 2 * gap * gap;
    return 1;
}

/* 平方探测和双散列按均匀探测：成功 -(1/α)ln(1-α)，失败 1/(1-α)。 */
static int formula_uniform(long long n, long long m, HashFormula *out) {
    double alpha;
    if (n <= 0 || n >= m) {
        return 0;
    }
    alpha = (double)n / (double)m;
    out->succ_dec = -log(1.0 - alpha) / alpha;
    out->fail_exact = 1;
    out->fail_num = m;
    out->fail_den = m - n;
    return 1;
}

static int formula_chain(long long n, long long m, HashFormula *out) {
    double alpha = (double)n / (double)m;
    if (n == 0) {
        out->succ_none = 1;
    } else {
        out->succ_exact = 1;
        out->succ_num = 2 * m + n;
        out->succ_den = 2 * m;
    }
    /* α + e^{-α} 是把空指针也算进去的近似，和「只数链长」不是同一个数。 */
    out->fail_dec = alpha + exp(-alpha);
    return 1;
}

int hash_formula(int kind, long long n, long long m, HashFormula *out) {
    memset(out, 0, sizeof *out);
    if (m < 1 || n < 0 || m > HASH_THEORY_MAX || n > HASH_THEORY_MAX) {
        return 0;
    }
    if (kind == KIND_LINEAR) {
        return formula_linear(n, m, out);
    }
    if (kind == KIND_QUAD || kind == KIND_QPOS || kind == KIND_DOUBLE) {
        return formula_uniform(n, m, out);
    }
    if (kind == KIND_CHAIN) {
        return formula_chain(n, m, out);
    }
    return 0;
}

int hash_run(const HashTab *t) {
    int occ[HASH_M_MAX];
    int all = 1;
    int best = 0;
    int cur = 0;
    int i;
    int pre = 0;
    int suf = 0;
    if (t->kind == KIND_CHAIN) {
        return -1;
    }
    for (i = 0; i < t->m; i++) {
        occ[i] = t->state[i] != CELL_EMPTY;
        if (!occ[i]) {
            all = 0;
        }
        if (occ[i]) {
            cur++;
            if (cur > best) {
                best = cur;
            }
        } else {
            cur = 0;
        }
    }
    if (all) {
        return t->m;
    }
    while (pre < t->m && occ[pre]) {
        pre++;
    }
    while (suf < t->m && occ[t->m - 1 - suf]) {
        suf++;
    }
    if (pre + suf <= t->m && pre + suf > best) {
        best = pre + suf;
    }
    return best;
}
