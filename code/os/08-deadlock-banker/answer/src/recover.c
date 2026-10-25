#include "recover.h"

#include "fmt.h"

#include <stdio.h>
#include <string.h>

/*
 * 只在还占着资源的进程里挑。手里是空的人被撤销后还不回资源，环拆不掉。
 * 代价相同取编号最小，王道没有规定并列时选谁，这样输出才唯一。
 */
static int pick_victim(const Graph *g, const RedTrace *t, const int *cost) {
    int i;
    int best = -1;
    for (i = 0; i < t->nleft; i++) {
        int who = t->left[i];
        if (!num_any(g->alloc[who], g->m)) {
            continue;
        }
        if (best < 0 || cost[who] < cost[best] || (cost[who] == cost[best] && who < best)) {
            best = who;
        }
    }
    return best;
}

static void abort_one(Graph *g, int who) {
    int next[BANK_M];
    num_add(next, g->avail, g->alloc[who], g->m);
    num_copy(g->avail, next, g->m);
    num_zero(g->alloc[who], g->m);
    num_zero(g->req[who], g->m);
}

int recover_run(const Graph *src, const int *cost, RecResult *out) {
    Graph g = *src;
    memset(out, 0, sizeof *out);
    while (out->nround <= g.n) {
        int slot = out->nround;
        int who;
        num_copy(out->avail0[slot], g.avail, g.m);
        graph_reduce(&g, &out->trace[slot]);
        out->nround++;
        if (!out->trace[slot].deadlock) {
            out->recovered = 1;
            return 0;
        }
        who = pick_victim(&g, &out->trace[slot], cost);
        if (who < 0) {
            out->no_holder = 1;
            return 0;
        }
        out->abort_who[out->naborts] = who;
        out->abort_cost[out->naborts] = cost[who];
        abort_one(&g, who);
        num_copy(out->avail_after[out->naborts], g.avail, g.m);
        out->naborts++;
    }
    return 0;
}

static void print_round_body(const Graph *g, const int *avail, const RedTrace *t) {
    char iso[128];
    char work[128];
    char removed[128];
    char left[128];
    int i;
    fmt_names(iso, (int)sizeof iso, t->isolated, t->niso, g->base);
    fmt_vec(work, (int)sizeof work, avail, g->m);
    fmt_names(removed, (int)sizeof removed, t->removed, t->nremoved, g->base);
    fmt_names(left, (int)sizeof left, t->left, t->nleft, g->base);
    printf("isolated=%s\n", iso);
    printf("work0=%s\n", work);
    if (t->nstep > 0) {
        printf("who work request alloc next\n");
    }
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
    if (t->deadlock) {
        char stuck[128];
        fmt_vec(stuck, (int)sizeof stuck, t->stuck, g->m);
        printf("stuck work=%s\n", stuck);
    }
    printf("deadlock=%s removed=%s left=%s\n", t->deadlock ? "yes" : "no", removed, left);
}

void recover_print(const Graph *g, const RecResult *out) {
    int i;
    int ai = 0;
    char aborted[128];
    for (i = 0; i < out->nround; i++) {
        printf("round %d\n", i + 1);
        print_round_body(g, out->avail0[i], &out->trace[i]);
        if (out->trace[i].deadlock && ai < out->naborts) {
            char name[16];
            char avail[128];
            fmt_name(name, (int)sizeof name, out->abort_who[ai], g->base);
            fmt_vec(avail, (int)sizeof avail, out->avail_after[ai], g->m);
            printf("abort who=%s cost=%d avail=%s\n", name, out->abort_cost[ai], avail);
            ai++;
        }
    }
    if (!out->recovered) {
        printf("result recovered=no reason=no-holder\n");
        return;
    }
    fmt_names(aborted, (int)sizeof aborted, out->abort_who, out->naborts, g->base);
    printf("result recovered=yes aborted=%s\n", aborted);
}
