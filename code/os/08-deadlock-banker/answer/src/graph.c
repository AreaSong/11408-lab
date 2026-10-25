#include "graph.h"

#include "fmt.h"

#include <stdio.h>
#include <string.h>

static int isolated_row(const Graph *g, int i) {
    return !num_any(g->alloc[i], g->m) && !num_any(g->req[i], g->m);
}

static int pick_free(const Graph *g, const int *finish, const int *work) {
    int i;
    for (i = 0; i < g->n; i++) {
        if (!finish[i] && num_leq(g->req[i], work, g->m)) {
            return i;
        }
    }
    return -1;
}

static void push_cut(const Graph *g, RedTrace *out, int who, int *work) {
    TraceStep *s = &out->steps[out->nstep];
    s->who = who;
    num_copy(s->work, work, g->m);
    num_copy(s->ask, g->req[who], g->m);
    num_copy(s->alloc, g->alloc[who], g->m);
    num_add(s->next, work, g->alloc[who], g->m);
    num_copy(work, s->next, g->m);
    out->removed[out->nremoved++] = who;
    out->nstep++;
}

/*
 * 资源分配图化简。申请向量是「此刻正在等的」，不是银行家的 Need。
 * 孤立结点没有申请边也没有分配边，本来就不参与环。
 * 申请能满足时，进程运行完会把占有的资源还回来；临时分给它的那一份也一起还，
 * 所以 Work 只加 Allocation。多个都能化简时取编号最小的。
 */
int graph_reduce(const Graph *g, RedTrace *out) {
    int finish[BANK_N];
    int work[BANK_M];
    int i;
    memset(out, 0, sizeof *out);
    num_copy(work, g->avail, g->m);
    for (i = 0; i < g->n; i++) {
        if (isolated_row(g, i)) {
            finish[i] = 1;
            out->isolated[out->niso++] = i;
        } else {
            finish[i] = 0;
        }
    }
    while (out->nstep < g->n) {
        int who = pick_free(g, finish, work);
        if (who < 0) {
            break;
        }
        push_cut(g, out, who, work);
        finish[who] = 1;
    }
    num_copy(out->stuck, work, g->m);
    for (i = 0; i < g->n; i++) {
        if (!finish[i]) {
            out->left[out->nleft++] = i;
        }
    }
    out->deadlock = out->nleft > 0;
    return 0;
}

static void print_cuts(const Graph *g, const RedTrace *t) {
    int i;
    if (t->nstep == 0) {
        return;
    }
    printf("who work request alloc next\n");
    for (i = 0; i < t->nstep; i++) {
        char name[16];
        char w[128];
        char q[128];
        char h[128];
        char nx[128];
        const TraceStep *s = &t->steps[i];
        fmt_name(name, (int)sizeof name, s->who, g->base);
        fmt_vec(w, (int)sizeof w, s->work, g->m);
        fmt_vec(q, (int)sizeof q, s->ask, g->m);
        fmt_vec(h, (int)sizeof h, s->alloc, g->m);
        fmt_vec(nx, (int)sizeof nx, s->next, g->m);
        printf("%s %s %s %s %s\n", name, w, q, h, nx);
    }
}

void graph_print(const Graph *g, const RedTrace *t) {
    char iso[128];
    char work[128];
    char removed[128];
    char left[128];
    fmt_names(iso, (int)sizeof iso, t->isolated, t->niso, g->base);
    fmt_vec(work, (int)sizeof work, g->avail, g->m);
    fmt_names(removed, (int)sizeof removed, t->removed, t->nremoved, g->base);
    fmt_names(left, (int)sizeof left, t->left, t->nleft, g->base);
    printf("algo=reduce rule=smallest\n");
    printf("isolated=%s\n", iso);
    printf("work0=%s\n", work);
    print_cuts(g, t);
    if (t->deadlock) {
        char stuck[128];
        fmt_vec(stuck, (int)sizeof stuck, t->stuck, g->m);
        printf("stuck work=%s\n", stuck);
    }
    printf("result deadlock=%s removed=%s left=%s\n", t->deadlock ? "yes" : "no", removed, left);
}
