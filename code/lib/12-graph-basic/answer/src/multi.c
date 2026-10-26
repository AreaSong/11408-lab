#include "multi.h"

#include <stdlib.h>
#include <string.h>

/* 邻接多重表只服务无向图。一条边一个结点，同时挂到两个端点的链上。
 * 顶点是 ivex 时走 ilink，是 jvex 时走 jlink。mark 用来保证遍历时每条边只处理一次，
 * 否则从两个端点会各看到它一次。 */
typedef struct EBox {
    int mark;
    int ivex;
    int jvex;
    struct EBox *ilink;
    struct EBox *jlink;
} EBox;

typedef struct {
    EBox *firstedge;
} Vex;

static EBox *next_of(const EBox *e, int vertex) {
    if (e->ivex == vertex) {
        return e->ilink;
    }
    return e->jlink;
}

static void set_next(EBox *e, int vertex, EBox *nxt) {
    if (e->ivex == vertex) {
        e->ilink = nxt;
    } else {
        e->jlink = nxt;
    }
}

static void attach(Vex *vx, int vertex, EBox *e) {
    EBox *p;
    if (e->ivex == vertex) {
        e->ilink = NULL;
    } else {
        e->jlink = NULL;
    }
    if (vx->firstedge == NULL) {
        vx->firstedge = e;
        return;
    }
    p = vx->firstedge;
    while (next_of(p, vertex) != NULL) {
        p = next_of(p, vertex);
    }
    set_next(p, vertex, e);
}

static int chain_len(const EBox *p, int vertex) {
    int count = 0;
    while (p != NULL) {
        count++;
        p = next_of(p, vertex);
    }
    return count;
}

static void print_chain(FILE *out, const EBox *p, int vertex) {
    int count = 0;
    if (p == NULL) {
        fputc('-', out);
        return;
    }
    while (p != NULL) {
        if (count > 0) {
            fputc(',', out);
        }
        fprintf(out, "%d-%d", p->ivex, p->jvex);
        count++;
        p = next_of(p, vertex);
    }
}

static void print_boxes(EBox **all, int nbox, FILE *out) {
    for (int i = 0; i < nbox; i++) {
        const EBox *e = all[i];
        fprintf(out, "box %d-%d inext=", e->ivex, e->jvex);
        if (e->ilink == NULL) {
            fputc('-', out);
        } else {
            fprintf(out, "%d-%d", e->ilink->ivex, e->ilink->jvex);
        }
        fprintf(out, " jnext=");
        if (e->jlink == NULL) {
            fputc('-', out);
        } else {
            fprintf(out, "%d-%d", e->jlink->ivex, e->jlink->jvex);
        }
        fputc('\n', out);
    }
}

static void count_marks(Vex *vex, int n, FILE *out) {
    int seen = 0;
    int once = 0;
    for (int u = 0; u < n; u++) {
        for (EBox *p = vex[u].firstedge; p != NULL; p = next_of(p, u)) {
            seen++;
            if (p->mark == 0) {
                p->mark = 1;
                once++;
            }
        }
    }
    fprintf(out, "seen-from-ends=%d\nmarked-once=%d\n", seen, once);
}

static void free_boxes(EBox **all, int nbox) {
    for (int i = 0; i < nbox; i++) {
        free(all[i]);
    }
}

int multi_print(const Graph *g, FILE *out) {
    Vex vex[G_MAX];
    EBox *all[G_MAX * G_MAX];
    int hits[G_MAX] = {0};
    int nbox = 0;
    if (g->kind != G_U) {
        return -1;
    }
    memset(vex, 0, sizeof vex);
    for (int i = 0; i < g->e; i++) {
        EBox *e = calloc(1, sizeof *e);
        if (e == NULL) {
            free_boxes(all, nbox);
            return -2;
        }
        e->ivex = g->eu[i];
        e->jvex = g->ev[i];
        all[nbox++] = e;
        attach(&vex[e->ivex], e->ivex, e);
        attach(&vex[e->jvex], e->jvex, e);
        hits[e->ivex]++;
        hits[e->jvex]++;
    }
    fprintf(out, "multi\nedge-nodes=%d\nlist-nodes=%d\n", g->e, graph_list_nodes(g));
    print_boxes(all, nbox, out);
    for (int u = 0; u < g->n; u++) {
        fprintf(out, "v%d deg=%d chain=", u, chain_len(vex[u].firstedge, u));
        print_chain(out, vex[u].firstedge, u);
        fputc('\n', out);
    }
    fprintf(out, "hits");
    for (int u = 0; u < g->n; u++) {
        fprintf(out, " %d=%d", u, hits[u]);
    }
    fputc('\n', out);
    /* 编号在 ivex/jvex 里出现几次，度就是几。2024 的邻接多重表就是这样数的。 */
    count_marks(vex, g->n, out);
    free_boxes(all, nbox);
    return 0;
}
