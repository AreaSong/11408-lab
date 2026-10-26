#include "hash.h"

#include <stdio.h>

static int gap_try(int kind, int *empty, int *put) {
    HashTab *t = hash_create(kind, 5, 5, 1, 0);
    int keys[3] = {0, 5, 10};
    int i;
    int rc;
    for (i = 0; i < 3; i++) {
        if (hash_insert(t, keys[i]) != HASH_OK) {
            hash_free(t);
            *empty = 0;
            return HASH_FULL;
        }
    }
    rc = hash_insert(t, 15);
    *empty = t->m - t->n;
    if (put != NULL) {
        OpenWalk w;
        *put = -1;
        if (rc == HASH_OK && open_find(t, 15, &w) == HASH_OK) {
            *put = w.put;
        }
    }
    hash_free(t);
    return rc;
}

/* 线性探测会撞上散列地址不同的关键字，所以冲突不都发生在同义词之间。 */
static int see_mix(void) {
    HashTab *t = hash_create(KIND_LINEAR, 7, 7, 1, 0);
    OpenWalk w;
    int mix = 0;
    int hnew;
    int i;
    hash_insert(t, 0);
    hash_insert(t, 1);
    open_insert(t, 7, &w);
    hnew = hash_addr(t, 7);
    for (i = 0; i < w.npath; i++) {
        if (w.cell_state[i] != CELL_FULL || w.pos[i] == w.put) {
            continue;
        }
        if (hash_addr(t, w.cell_key[i]) != hnew) {
            mix = 1;
        }
    }
    hash_free(t);
    return mix;
}

static int see_syn(void) {
    HashTab *t = hash_create(KIND_QUAD, 11, 11, 1, 0);
    OpenWalk w;
    int syn = 0;
    int hnew;
    int i;
    hash_insert(t, 1);
    open_insert(t, 12, &w);
    hnew = hash_addr(t, 12);
    for (i = 0; i < w.npath; i++) {
        if (w.cell_state[i] == CELL_FULL && hash_addr(t, w.cell_key[i]) == hnew) {
            syn = 1;
        }
    }
    hash_free(t);
    return syn;
}

static void clash_letter(int a_holds, int b_holds, int c_holds, int d_holds) {
    if (a_holds && !b_holds && !c_holds && !d_holds) {
        printf("letter=A\n");
    } else {
        printf("letter=none\n");
    }
}

void fact_clash(void) {
    int lin_empty = 0;
    int lin_put = -1;
    int quad_empty = 0;
    int qpos_empty = 0;
    int lin_rc = gap_try(KIND_LINEAR, &lin_empty, &lin_put);
    int quad_rc = gap_try(KIND_QUAD, &quad_empty, NULL);
    int qpos_rc = gap_try(KIND_QPOS, &qpos_empty, NULL);
    int mix = see_mix();
    int syn = see_syn();
    int counter;
    printf("linear empty=%d put=%d\n", lin_empty, lin_put);
    printf("linear_gap=%s\n", lin_rc == HASH_OK ? "yes" : "no");
    printf("quad empty=%d\n", quad_empty);
    printf("quad_gap=%s\n", quad_rc == HASH_OK ? "yes" : "no");
    printf("qpos empty=%d\n", qpos_empty);
    printf("qpos_gap=%s\n", qpos_rc == HASH_OK ? "yes" : "no");
    printf("linear_mix=%s\n", mix ? "yes" : "no");
    printf("quad_syn=%s\n", syn ? "yes" : "no");
    counter = (quad_rc == HASH_FULL && quad_empty > 0) || (qpos_rc == HASH_FULL && qpos_empty > 0);
    clash_letter(lin_rc == HASH_OK && lin_empty > 0, !counter, !mix, !syn);
}

static int frac_gt(long long a, long long b, long long c, long long d) {
    return a * d > c * b;
}

static void load_seven(HashTab *t) {
    int k;
    for (k = 1; k <= 7; k++) {
        hash_insert(t, k);
    }
}

static char letter_iii(int i_yes, int ii_yes, int iii_yes) {
    if (i_yes && !ii_yes && !iii_yes) {
        return 'A';
    }
    if (!i_yes && ii_yes && !iii_yes) {
        return 'B';
    }
    if (i_yes && ii_yes && !iii_yes) {
        return 'C';
    }
    if (!i_yes && ii_yes && iii_yes) {
        return 'D';
    }
    return '?';
}

static void improve_alpha(int *i_yes) {
    HashFormula hi;
    HashFormula lo;
    hash_formula(KIND_LINEAR, 7, 10, &hi);
    hash_formula(KIND_LINEAR, 7, 20, &lo);
    hash_print_frac("alpha_7_10", hi.succ_num, hi.succ_den);
    hash_print_frac("alpha_7_20", lo.succ_num, lo.succ_den);
    *i_yes = frac_gt(lo.succ_num, lo.succ_den, hi.succ_num, hi.succ_den);
}

static void improve_hash(int *ii_yes) {
    HashTab *good = hash_create(KIND_LINEAR, 13, 7, 1, 0);
    HashTab *bad = hash_create(KIND_LINEAR, 13, 7, 7, 0);
    HashAsl gs;
    HashAsl bs;
    load_seven(good);
    load_seven(bad);
    hash_asl(good, 0, &gs);
    hash_asl(bad, 0, &bs);
    hash_print_frac("hash_a1", gs.succ_num, gs.succ_den);
    hash_print_frac("hash_a7", bs.succ_num, bs.succ_den);
    *ii_yes = frac_gt(bs.succ_num, bs.succ_den, gs.succ_num, gs.succ_den);
    hash_free(good);
    hash_free(bad);
}

static void improve_crowd(int *iii_yes) {
    HashTab *lin = hash_create(KIND_LINEAR, 13, 7, 7, 0);
    HashTab *ch = hash_create(KIND_CHAIN, 13, 7, 7, 0);
    HashAsl ls;
    HashAsl cs;
    load_seven(lin);
    load_seven(ch);
    hash_asl(lin, 0, &ls);
    hash_asl(ch, 0, &cs);
    hash_print_frac("linear_fail", ls.fail_num, ls.fail_den);
    hash_print_frac("chain_fail", cs.fail_num, cs.fail_den);
    *iii_yes = frac_gt(ls.fail_num, ls.fail_den, cs.fail_num, cs.fail_den);
    hash_free(lin);
    hash_free(ch);
}

void fact_improve(void) {
    int i_yes = 0;
    int ii_yes = 0;
    int iii_yes = 0;
    char letter;
    improve_alpha(&i_yes);
    printf("I=%s\n", i_yes ? "yes" : "no");
    improve_hash(&ii_yes);
    printf("II=%s\n", ii_yes ? "yes" : "no");
    improve_crowd(&iii_yes);
    printf("III=%s\n", iii_yes ? "yes" : "no");
    letter = letter_iii(i_yes, ii_yes, iii_yes);
    if (letter == '?') {
        printf("letter=none\n");
    } else {
        printf("letter=%c\n", letter);
    }
}

static void grow_formula(int kind, int n, int m) {
    HashFormula f;
    if (!hash_formula(kind, n, m, &f)) {
        printf("formula=invalid\n");
        return;
    }
    if (f.succ_exact) {
        hash_print_frac("formula_succ", f.succ_num, f.succ_den);
    } else {
        printf("formula_succ=%.4f\n", f.succ_dec);
    }
    if (f.fail_exact) {
        hash_print_frac("formula_fail", f.fail_num, f.fail_den);
    } else {
        printf("formula_fail=%.4f\n", f.fail_dec);
    }
}

void fact_grow(int kind, int p, int n, int m) {
    HashTab *t = hash_create(kind, m, p, 1, 0);
    HashAsl asl;
    long long g;
    int k;
    printf("grow %s p=%d n=%d m=%d\n", hash_kind_name(kind), p, n, m);
    if (t == NULL) {
        printf("fail range\n");
        return;
    }
    for (k = 1; k <= n; k++) {
        if (hash_insert(t, k) != HASH_OK) {
            printf("fail full\n");
            hash_free(t);
            return;
        }
    }
    hash_asl(t, 0, &asl);
    g = hash_gcd(t->n, t->m);
    printf("alpha=%lld/%lld\n", (long long)t->n / g, (long long)t->m / g);
    hash_print_frac("success", asl.succ_num, asl.succ_den);
    if (asl.depend) {
        printf("fail=depend\n");
    } else {
        hash_print_frac("fail", asl.fail_num, asl.fail_den);
    }
    grow_formula(kind, n, m);
    hash_free(t);
}
