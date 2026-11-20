#include "hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *hash_kind_name(int kind) {
    switch (kind) {
    case KIND_LINEAR:
        return "linear";
    case KIND_QUAD:
        return "quad";
    case KIND_QPOS:
        return "qpos";
    case KIND_DOUBLE:
        return "double";
    case KIND_CHAIN:
        return "chain";
    default:
        return "none";
    }
}

int hash_kind_from(const char *name) {
    if (strcmp(name, "linear") == 0) {
        return KIND_LINEAR;
    }
    if (strcmp(name, "quad") == 0) {
        return KIND_QUAD;
    }
    if (strcmp(name, "qpos") == 0) {
        return KIND_QPOS;
    }
    if (strcmp(name, "double") == 0) {
        return KIND_DOUBLE;
    }
    if (strcmp(name, "chain") == 0) {
        return KIND_CHAIN;
    }
    return KIND_NONE;
}

static void *need(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (p == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return p;
}

HashTab *hash_create(int kind, int m, int p, int a, int q) {
    HashTab *t;
    if (m < 1 || m > HASH_M_MAX || p < 1 || p > m || a < 1) {
        return NULL;
    }
    t = need(1, sizeof *t);
    t->kind = kind;
    t->m = m;
    t->p = p;
    t->a = a;
    t->q = q;
    t->key = need((size_t)m, sizeof(int));
    t->state = need((size_t)m, sizeof(int));
    if (kind == KIND_CHAIN) {
        t->head = need((size_t)m, sizeof(HashNode *));
    }
    return t;
}

void hash_free(HashTab *t) {
    int i;
    if (t == NULL) {
        return;
    }
    if (t->head != NULL) {
        for (i = 0; i < t->m; i++) {
            HashNode *p = t->head[i];
            while (p != NULL) {
                HashNode *next = p->next;
                free(p);
                p = next;
            }
        }
    }
    free(t->head);
    free(t->key);
    free(t->state);
    free(t);
}

int hash_addr(const HashTab *t, int key) {
    return (int)(((long long)t->a * key + t->b) % t->p);
}

/* H2 = q - key%q，落在 1..q。q < m 时步长不会是 0，但未必和 m 互质。 */
int hash_step(const HashTab *t, int key) {
    return t->q - key % t->q;
}

static int mod_m(long long x, int m) {
    long long r = x % m;
    if (r < 0) {
        r += m;
    }
    return (int)r;
}

/*
 * k 和 k+m 的平方模 m 相同，所以 k 从 1 试到 m-1 就覆盖了全部平方增量。
 * 双向时每个 k 先取正再取负，和王道的 1^2、-1^2、2^2、-2^2 一致。
 * 平方增量生成不了的地址直接不放进序列，调用方会在序列走完时判定插入失败。
 */
static int probe_square(int h0, int m, int both, int *pos) {
    int seen[HASH_M_MAX] = {0};
    int n = 0;
    int first = mod_m(h0, m);
    int k;
    pos[n++] = first;
    seen[first] = 1;
    for (k = 1; k < m; k++) {
        long long sq = (long long)k * (long long)k;
        int signs = both ? 2 : 1;
        int s;
        for (s = 0; s < signs; s++) {
            int sign = (s == 0) ? 1 : -1;
            int p = mod_m((long long)h0 + (long long)sign * sq, m);
            if (seen[p]) {
                continue;
            }
            seen[p] = 1;
            pos[n++] = p;
            if (n == m) {
                return n;
            }
        }
    }
    return n;
}

static int probe_step(int h0, int m, int step, int *pos) {
    int seen[HASH_M_MAX] = {0};
    int n = 0;
    int i;
    if (step % m == 0) {
        return 0;
    }
    for (i = 0; i < m; i++) {
        int p = mod_m((long long)h0 + (long long)i * step, m);
        if (seen[p]) {
            break;
        }
        seen[p] = 1;
        pos[n++] = p;
    }
    return n;
}

int hash_probe(const HashTab *t, int h0, int step, int *pos) {
    if (t->m < 1 || t->m > HASH_M_MAX) {
        return 0;
    }
    if (t->kind == KIND_QUAD) {
        return probe_square(h0, t->m, 1, pos);
    }
    if (t->kind == KIND_QPOS) {
        return probe_square(h0, t->m, 0, pos);
    }
    if (t->kind == KIND_DOUBLE) {
        return probe_step(h0, t->m, step, pos);
    }
    return probe_step(h0, t->m, 1, pos);
}

int hash_insert(HashTab *t, int key) {
    if (t->kind == KIND_CHAIN) {
        ChainWalk w;
        return chain_insert(t, key, &w);
    }
    OpenWalk w;
    return open_insert(t, key, &w);
}
