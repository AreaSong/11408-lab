#include "enumdfs.h"

#include "walk.h"

#include <stdlib.h>
#include <string.h>

/* 邻接表次序不唯一。每个顶点的邻接排列数是度数的阶乘，全体是这些阶乘的乘积。
 * 超过 ENUM_CAP 就拒绝，完全图的全排列会非常大。 */
#define ENUM_CAP 20000

typedef struct {
    int n;
    int v[G_MAX];
} Seq;

typedef struct {
    int n;
    int start;
    int len[G_MAX];
    int adj[G_MAX][G_MAX];
    Seq *rows;
    int total;
} EnumCtx;

static int seq_cmp(const void *a, const void *b) {
    const Seq *x = a;
    const Seq *y = b;
    for (int i = 0; i < x->n; i++) {
        if (x->v[i] != y->v[i]) {
            return x->v[i] - y->v[i];
        }
    }
    return 0;
}

static int perm_count(const Graph *g, long long *out) {
    long long p = 1;
    for (int v = 0; v < g->n; v++) {
        for (int i = 2; i <= g->len[v]; i++) {
            if (p > ENUM_CAP / i) {
                return 0;
            }
            p *= i;
        }
    }
    *out = p;
    return 1;
}

static void swap_int(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

static void perms_from(EnumCtx *c, int v);

static void permute(EnumCtx *c, int v, int i) {
    if (i == c->len[v]) {
        perms_from(c, v + 1);
        return;
    }
    for (int j = i; j < c->len[v]; j++) {
        swap_int(&c->adj[v][i], &c->adj[v][j]);
        permute(c, v, i + 1);
        swap_int(&c->adj[v][i], &c->adj[v][j]);
    }
}

static void perms_from(EnumCtx *c, int v) {
    if (v == c->n) {
        Seq *s = &c->rows[c->total++];
        s->n = c->n;
        adj_dfs_cover(c->n, c->len, c->adj, c->start, s->v);
        return;
    }
    permute(c, v, 0);
}

static int unique_seqs(Seq *rows, int total) {
    int uniq = 1;
    if (total == 0) {
        return 0;
    }
    for (int i = 1; i < total; i++) {
        if (seq_cmp(&rows[i], &rows[uniq - 1]) != 0) {
            rows[uniq++] = rows[i];
        }
    }
    return uniq;
}

static void print_seqs(const Seq *rows, int total, int uniq, FILE *out) {
    fprintf(out, "perms=%d\ncount=%d\n", total, uniq);
    for (int i = 0; i < uniq; i++) {
        for (int j = 0; j < rows[i].n; j++) {
            if (j > 0) {
                fputc(' ', out);
            }
            fprintf(out, "%d", rows[i].v[j]);
        }
        fputc('\n', out);
    }
}

static void copy_adj(EnumCtx *c, const Graph *g) {
    c->n = g->n;
    for (int u = 0; u < g->n; u++) {
        c->len[u] = g->len[u];
        for (int i = 0; i < g->len[u]; i++) {
            c->adj[u][i] = g->adj[u][i];
        }
    }
}

int enum_dfs(const Graph *g, int start, FILE *out) {
    EnumCtx ctx;
    long long perms = 0;
    int uniq;
    memset(&ctx, 0, sizeof ctx);
    if (start < 0 || start >= g->n || !perm_count(g, &perms)) {
        return -1;
    }
    ctx.rows = malloc((size_t)perms * sizeof *ctx.rows);
    if (ctx.rows == NULL) {
        return -2;
    }
    ctx.start = start;
    copy_adj(&ctx, g);
    perms_from(&ctx, 0);
    qsort(ctx.rows, (size_t)ctx.total, sizeof *ctx.rows, seq_cmp);
    uniq = unique_seqs(ctx.rows, ctx.total);
    print_seqs(ctx.rows, ctx.total, uniq, out);
    free(ctx.rows);
    return 0;
}
