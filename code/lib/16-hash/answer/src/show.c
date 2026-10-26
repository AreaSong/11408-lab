#include "hash.h"

#include <stdio.h>

static void open_path(const OpenWalk *w) {
    int i;
    printf("path");
    for (i = 0; i < w->npath; i++) {
        printf(" %d:", w->pos[i]);
        if (w->cell_state[i] == CELL_EMPTY) {
            printf("-");
        } else if (w->cell_state[i] == CELL_TOMB) {
            printf("#");
        } else {
            printf("%d", w->cell_key[i]);
        }
    }
    printf("\n");
}

static void chain_path(const ChainWalk *w) {
    int i;
    printf("path");
    if (w->npath == 0) {
        printf(" -");
    }
    for (i = 0; i < w->npath; i++) {
        printf(" %d", w->key[i]);
    }
    printf("\n");
}

void show_open_ins(int key, const OpenWalk *w) {
    printf("ins %d h=%d\n", key, w->h);
    open_path(w);
    if (w->rc == HASH_OK) {
        printf("put %d cmp=%d\n", w->put, w->cmp);
    } else if (w->rc == HASH_DUP) {
        printf("fail dup\n");
    } else {
        printf("fail full\n");
    }
}

void show_open_query(const char *cmd, int key, const OpenWalk *w) {
    printf("%s %d h=%d\n", cmd, key, w->h);
    open_path(w);
    if (w->rc == HASH_OK) {
        printf("hit pos=%d cmp=%d\n", w->put, w->cmp);
    } else {
        printf("miss stop=%d cmp=%d\n", w->put, w->cmp);
    }
}

void show_open_del(const char *cmd, int key, const OpenWalk *w, int wipe) {
    printf("%s %d h=%d\n", cmd, key, w->h);
    open_path(w);
    if (w->rc != HASH_OK) {
        printf("fail miss\n");
        return;
    }
    printf("%s %d\n", wipe ? "wipe" : "mark", w->put);
}

void show_chain_ins(int key, const ChainWalk *w) {
    printf("ins %d h=%d\n", key, w->h);
    chain_path(w);
    if (w->rc == HASH_OK) {
        printf("front cmp=%d\n", w->cmp);
    } else if (w->rc == HASH_DUP) {
        printf("fail dup\n");
    } else {
        printf("fail full\n");
    }
}

void show_chain_query(const char *cmd, int key, const ChainWalk *w) {
    printf("%s %d h=%d\n", cmd, key, w->h);
    chain_path(w);
    if (w->rc == HASH_OK) {
        printf("hit cmp=%d\n", w->cmp);
    } else {
        printf("miss cmp=%d\n", w->cmp);
    }
}

void show_chain_del(int key, const ChainWalk *w) {
    printf("del %d h=%d\n", key, w->h);
    chain_path(w);
    if (w->rc == HASH_OK) {
        printf("drop cmp=%d\n", w->cmp);
    } else {
        printf("fail miss\n");
    }
}

static void show_slots(const HashTab *t) {
    int i;
    if (t->kind == KIND_CHAIN) {
        for (i = 0; i < t->m; i++) {
            HashNode *p;
            printf("bucket %d", i);
            if (t->head[i] == NULL) {
                printf(" -");
            }
            for (p = t->head[i]; p != NULL; p = p->next) {
                printf(" %d", p->key);
            }
            printf("\n");
        }
        return;
    }
    printf("slot");
    for (i = 0; i < t->m; i++) {
        printf(" ");
        if (t->state[i] == CELL_EMPTY) {
            printf("-");
        } else if (t->state[i] == CELL_TOMB) {
            printf("#");
        } else {
            printf("%d", t->key[i]);
        }
    }
    printf("\n");
}

void show_asl(const HashTab *t, int count_null) {
    HashAsl a;
    hash_asl(t, count_null, &a);
    if (!a.has_succ) {
        printf("success=none\n");
    } else {
        hash_print_frac("success", a.succ_num, a.succ_den);
    }
    if (a.depend) {
        printf("fail=depend\n");
    } else {
        hash_print_frac("fail", a.fail_num, a.fail_den);
    }
    printf("den=%d\n", t->p);
    /* 开放定址的空位本来就要比一次，null 开关只对拉链法有意义。 */
    if (t->kind != KIND_CHAIN) {
        printf("null=open\n");
        return;
    }
    printf("null=%s\n", count_null ? "on" : "off");
}

void show_summary(const HashTab *t) {
    HashAsl a;
    long long g = hash_gcd(t->n, t->m);
    printf("kind=%s\n", hash_kind_name(t->kind));
    printf("m=%d p=%d a=%d", t->m, t->p, t->a);
    if (t->b != 0) {
        printf(" b=%d", t->b);
    }
    if (t->kind == KIND_DOUBLE) {
        printf(" q=%d", t->q);
    }
    printf("\n");
    show_slots(t);
    printf("n=%d\n", t->n);
    printf("alpha=%lld/%lld\n", (long long)t->n / g, (long long)t->m / g);
    hash_asl(t, 0, &a);
    if (!a.has_succ) {
        printf("success=none\n");
    } else {
        hash_print_frac("success", a.succ_num, a.succ_den);
    }
    if (a.depend) {
        printf("fail=depend\n");
    } else {
        hash_print_frac("fail", a.fail_num, a.fail_den);
    }
    printf("den=%d\n", t->p);
    if (t->kind == KIND_CHAIN) {
        printf("null=off\n");
    }
}

static void show_one_formula(const char *name, int none, int exact, long long num, long long den,
                              double dec) {
    if (none) {
        printf("%s=none\n", name);
    } else if (exact) {
        hash_print_frac(name, num, den);
    } else {
        printf("%s=%.4f\n", name, dec);
    }
}

void show_formula(int kind, long long n, long long m) {
    HashFormula f;
    long long g = hash_gcd(n, m);
    printf("theory %s n=%lld m=%lld\n", hash_kind_name(kind), n, m);
    printf("alpha=%lld/%lld\n", n / g, m / g);
    if (!hash_formula(kind, n, m, &f)) {
        printf("formula=invalid\n");
        return;
    }
    show_one_formula("formula_succ", f.succ_none, f.succ_exact, f.succ_num, f.succ_den, f.succ_dec);
    show_one_formula("formula_fail", 0, f.fail_exact, f.fail_num, f.fail_den, f.fail_dec);
}
