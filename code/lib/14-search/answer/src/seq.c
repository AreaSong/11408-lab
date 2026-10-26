#include "seq.h"

#include <string.h>

int seq_kind(const char *name) {
    if (strcmp(name, "plain") == 0) {
        return SEQ_PLAIN;
    }
    if (strcmp(name, "guard") == 0) {
        return SEQ_GUARD;
    }
    if (strcmp(name, "order") == 0) {
        return SEQ_ORDER;
    }
    return -1;
}

static int key_ok(int key) {
    return key >= KEY_LO && key <= KEY_HI;
}

int seq_load(SeqTable *t, int kind, const int *keys, int n) {
    int i;
    if (kind < SEQ_PLAIN || kind > SEQ_ORDER || n < 1 || n > TABLE_MAX) {
        return S_RANGE;
    }
    for (i = 0; i < n; i++) {
        if (!key_ok(keys[i])) {
            return S_RANGE;
        }
        /* 有序表用来提前结束失败查找，重复关键字会把区间切没，这里要求严格升序。 */
        if (kind == SEQ_ORDER && i > 0 && keys[i] <= keys[i - 1]) {
            return S_ORDER;
        }
    }
    t->kind = kind;
    t->n = n;
    t->weighted = 0;
    for (i = 1; i <= n; i++) {
        t->key[i] = keys[i - 1];
        t->weight[i] = 1;
    }
    return S_OK;
}

int seq_weight(SeqTable *t, const int *w, int n) {
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

static const char *kind_name(int kind) {
    if (kind == SEQ_GUARD) {
        return "guard";
    }
    if (kind == SEQ_ORDER) {
        return "order";
    }
    return "plain";
}

/* 从头数，第 i 个比 i 次。哨兵从尾数，下标 i 要比 n-i+1 次，平均起来一样。 */
static int succ_cmp(const SeqTable *t, int index) {
    if (t->kind == SEQ_GUARD) {
        return t->n - index + 1;
    }
    return index;
}

void seq_asl(const SeqTable *t, AslPair *a) {
    int i;
    long long succ_n = 0;
    long long succ_d = 0;
    for (i = 1; i <= t->n; i++) {
        long long w = t->weighted ? t->weight[i] : 1;
        succ_n += w * succ_cmp(t, i);
        succ_d += w;
    }
    a->succ_n = succ_n;
    a->succ_d = succ_d;
    if (t->kind == SEQ_GUARD) {
        /* 最后还要和 0 号哨兵比一次，失败只有这一种走法。 */
        a->fail_n = (long long)t->n + 1;
        a->fail_d = 1;
    } else if (t->kind == SEQ_ORDER) {
        /* n 个关键字切出 n+1 段。最后一段比完整个表就停，是 n 次，不是 n+1。 */
        a->fail_n = (long long)t->n * (t->n + 3) / 2;
        a->fail_d = (long long)t->n + 1;
    } else {
        a->fail_n = t->n;
        a->fail_d = 1;
    }
}

void seq_print_asl(const SeqTable *t, FILE *out) {
    AslPair a;
    seq_asl(t, &a);
    fprintf(out, "asl succ=");
    asl_write(out, a.succ_n, a.succ_d);
    fprintf(out, " fail=");
    asl_write(out, a.fail_n, a.fail_d);
    fprintf(out, " weight=%s\n", t->weighted ? "yes" : "no");
}

static void print_gap(const SeqTable *t, FILE *out) {
    int i;
    fprintf(out, "gap");
    if (t->kind == SEQ_ORDER) {
        for (i = 1; i <= t->n; i++) {
            fprintf(out, " %d", i);
        }
        fprintf(out, " %d", t->n);
    } else if (t->kind == SEQ_GUARD) {
        fprintf(out, " %d", t->n + 1);
    } else {
        fprintf(out, " %d", t->n);
    }
    fprintf(out, "\n");
}

void seq_report(const SeqTable *t, FILE *out) {
    int i;
    fprintf(out, "seq %s n=%d\n", kind_name(t->kind), t->n);
    fprintf(out, "cmp");
    for (i = 1; i <= t->n; i++) {
        fprintf(out, " %d", succ_cmp(t, i));
    }
    fprintf(out, "\n");
    print_gap(t, out);
    seq_print_asl(t, out);
}

static int find_guard(const SeqTable *t, int key, FILE *out, int *cmp) {
    int i;
    int ncmp = 0;
    for (i = t->n; i >= 1; i--) {
        int hit = t->key[i] == key;
        ncmp++;
        if (out != NULL) {
            fprintf(out, "i=%d a=%d %s\n", i, t->key[i], hit ? "=" : "!=");
        }
        if (hit) {
            if (out != NULL) {
                fprintf(out, "hit i=%d cmp=%d\n", i, ncmp);
            }
            *cmp = ncmp;
            return i;
        }
    }
    /* 哨兵一定相等。多出来的这一次就是失败 ASL 比无哨兵大 1 的原因。 */
    ncmp++;
    if (out != NULL) {
        fprintf(out, "i=0 sentinel =\n");
        fprintf(out, "miss cmp=%d\n", ncmp);
    }
    *cmp = ncmp;
    return 0;
}

static int find_plain(const SeqTable *t, int key, FILE *out, int *cmp) {
    int i;
    int ncmp = 0;
    for (i = 1; i <= t->n; i++) {
        int hit = t->key[i] == key;
        ncmp++;
        if (out != NULL) {
            fprintf(out, "i=%d a=%d %s\n", i, t->key[i], hit ? "=" : "!=");
        }
        if (hit) {
            if (out != NULL) {
                fprintf(out, "hit i=%d cmp=%d\n", i, ncmp);
            }
            *cmp = ncmp;
            return i;
        }
    }
    if (out != NULL) {
        fprintf(out, "miss cmp=%d\n", ncmp);
    }
    *cmp = ncmp;
    return 0;
}

static int find_order(const SeqTable *t, int key, FILE *out, int *cmp) {
    int i;
    int ncmp = 0;
    for (i = 1; i <= t->n; i++) {
        ncmp++;
        if (t->key[i] == key) {
            if (out != NULL) {
                fprintf(out, "i=%d a=%d =\n", i, t->key[i]);
                fprintf(out, "hit i=%d cmp=%d\n", i, ncmp);
            }
            *cmp = ncmp;
            return i;
        }
        if (t->key[i] > key) {
            if (out != NULL) {
                fprintf(out, "i=%d a=%d <\n", i, t->key[i]);
                fprintf(out, "miss cmp=%d\n", ncmp);
            }
            *cmp = ncmp;
            return 0;
        }
        if (out != NULL) {
            fprintf(out, "i=%d a=%d >\n", i, t->key[i]);
        }
    }
    if (out != NULL) {
        fprintf(out, "miss cmp=%d\n", ncmp);
    }
    *cmp = ncmp;
    return 0;
}

int seq_find(const SeqTable *t, int key, FILE *out, int *cmp) {
    if (t->kind == SEQ_GUARD) {
        return find_guard(t, key, out, cmp);
    }
    if (t->kind == SEQ_ORDER) {
        return find_order(t, key, out, cmp);
    }
    return find_plain(t, key, out, cmp);
}
