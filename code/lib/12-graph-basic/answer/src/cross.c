#include "cross.h"

#include <stdlib.h>
#include <string.h>

/* 十字链表只服务有向图：一条弧同时挂在弧尾的出边链和弧头的入边链上。
 * tlink 串同一弧尾，hlink 串同一弧头。弧结点个数是 e，不是 2e。
 * in_tail、out_tail 只为尾插，不是考纲字段。 */
typedef struct ArcBox {
    int tailvex;
    int headvex;
    struct ArcBox *hlink;
    struct ArcBox *tlink;
} ArcBox;

typedef struct {
    ArcBox *firstin;
    ArcBox *firstout;
    ArcBox *in_tail;
    ArcBox *out_tail;
} OLVex;

static void append_out(OLVex *vx, ArcBox *b) {
    b->tlink = NULL;
    if (vx->out_tail != NULL) {
        vx->out_tail->tlink = b;
    } else {
        vx->firstout = b;
    }
    vx->out_tail = b;
}

static void append_in(OLVex *vx, ArcBox *b) {
    b->hlink = NULL;
    if (vx->in_tail != NULL) {
        vx->in_tail->hlink = b;
    } else {
        vx->firstin = b;
    }
    vx->in_tail = b;
}

static int print_out_ids(FILE *out, const ArcBox *p) {
    int count = 0;
    if (p == NULL) {
        fputc('-', out);
        return 0;
    }
    for (; p != NULL; p = p->tlink) {
        if (count > 0) {
            fputc(',', out);
        }
        fprintf(out, "%d", p->headvex);
        count++;
    }
    return count;
}

static int print_in_ids(FILE *out, const ArcBox *p) {
    int count = 0;
    if (p == NULL) {
        fputc('-', out);
        return 0;
    }
    for (; p != NULL; p = p->hlink) {
        if (count > 0) {
            fputc(',', out);
        }
        fprintf(out, "%d", p->tailvex);
        count++;
    }
    return count;
}

static void print_box_end(FILE *out, const ArcBox *b) {
    if (b == NULL) {
        fputc('-', out);
        return;
    }
    fprintf(out, "%d>%d", b->tailvex, b->headvex);
}

static void print_boxes(ArcBox **all, int nbox, FILE *out) {
    for (int i = 0; i < nbox; i++) {
        const ArcBox *b = all[i];
        fprintf(out, "box %d>%d tnext=", b->tailvex, b->headvex);
        print_box_end(out, b->tlink);
        fprintf(out, " hnext=");
        print_box_end(out, b->hlink);
        fputc('\n', out);
    }
}

static void free_boxes(ArcBox **all, int nbox) {
    for (int i = 0; i < nbox; i++) {
        free(all[i]);
    }
}

int cross_print(const Graph *g, FILE *out) {
    OLVex vex[G_MAX];
    ArcBox *all[G_MAX * G_MAX];
    int outd[G_MAX] = {0};
    int ind[G_MAX] = {0};
    int nbox = 0;
    if (g->kind != G_D) {
        return -1;
    }
    memset(vex, 0, sizeof vex);
    for (int i = 0; i < g->e; i++) {
        ArcBox *b = calloc(1, sizeof *b);
        if (b == NULL) {
            free_boxes(all, nbox);
            return -2;
        }
        b->tailvex = g->eu[i];
        b->headvex = g->ev[i];
        all[nbox++] = b;
        append_out(&vex[b->tailvex], b);
        append_in(&vex[b->headvex], b);
    }
    fprintf(out, "cross\narc-nodes=%d\nlist-nodes=%d\n", g->e, graph_list_nodes(g));
    for (int u = 0; u < g->n; u++) {
        fprintf(out, "v%d out=", u);
        outd[u] = print_out_ids(out, vex[u].firstout);
        fprintf(out, " in=");
        ind[u] = print_in_ids(out, vex[u].firstin);
        fputc('\n', out);
    }
    print_boxes(all, nbox, out);
    fprintf(out, "out");
    for (int u = 0; u < g->n; u++) {
        fprintf(out, " %d", outd[u]);
    }
    fprintf(out, "\nin");
    for (int u = 0; u < g->n; u++) {
        fprintf(out, " %d", ind[u]);
    }
    fputc('\n', out);
    free_boxes(all, nbox);
    return 0;
}
