#include "walk.h"

#include "cycq.h"
#include "sqstack.h"

typedef struct {
    int visits;
    int scans;
    int starts;
} Tally;

typedef struct {
    const int *len;
    const int (*adj)[G_MAX];
    int *mark;
    int *order;
    int nord;
} Cover;

typedef struct {
    const Graph *g;
    int *mark;
    int *order;
    int nord;
    int *tu;
    int *tv;
    int nt;
    Tally *tally;
    FILE *out;
} WalkCtx;

static char link_ch(GKind kind) {
    return kind == G_D ? '>' : '-';
}

static void print_queue(const CycQ *q, FILE *out) {
    int i = q->front;
    int first = 1;
    fputc('[', out);
    while (i != q->rear) {
        if (!first) {
            fputc(' ', out);
        }
        first = 0;
        fprintf(out, "%d", q->data[i]);
        i = (i + 1) % q->cap;
    }
    fputc(']', out);
}

static void print_order(const int *order, int n, FILE *out) {
    fprintf(out, "order");
    for (int i = 0; i < n; i++) {
        fprintf(out, " %d", order[i]);
    }
    fputc('\n', out);
}

static void print_tree(const int *u, const int *v, int n, char ch, FILE *out) {
    fprintf(out, "tree");
    if (n == 0) {
        fprintf(out, " -");
    }
    for (int i = 0; i < n; i++) {
        fprintf(out, " %d%c%d", u[i], ch, v[i]);
    }
    fputc('\n', out);
}

static void show_enq(CycQ *q, int v, FILE *out) {
    cyc_enq(q, v);
    fprintf(out, "enq %d queue=", v);
    print_queue(q, out);
    fputc('\n', out);
}

static void push_show(SqStack *s, int v, FILE *out) {
    sq_push(s, v);
    fprintf(out, "push %d stack=", v);
    sq_print(s, out);
    fputc('\n', out);
}

static void pop_show(const SqStack *s, int v, FILE *out) {
    fprintf(out, "pop %d stack=", v);
    sq_print(s, out);
    fputc('\n', out);
}

static void cover_rec(Cover *c, int v) {
    c->mark[v] = 1;
    if (c->order != NULL) {
        c->order[c->nord++] = v;
    }
    for (int i = 0; i < c->len[v]; i++) {
        int w = c->adj[v][i];
        if (!c->mark[w]) {
            cover_rec(c, w);
        }
    }
}

int adj_component_starts(int n, const int *len, const int adj[][G_MAX]) {
    int mark[G_MAX] = {0};
    Cover c = {len, adj, mark, NULL, 0};
    int starts = 0;
    for (int v = 0; v < n; v++) {
        if (mark[v]) {
            continue;
        }
        starts++;
        cover_rec(&c, v);
    }
    return starts;
}

void adj_dfs_cover(int n, const int *len, const int adj[][G_MAX], int start, int *order) {
    int mark[G_MAX] = {0};
    Cover c = {len, adj, mark, order, 0};
    cover_rec(&c, start);
    for (int v = 0; v < n; v++) {
        if (!mark[v]) {
            cover_rec(&c, v);
        }
    }
}

static int reachable(const Graph *g, int src) {
    int mark[G_MAX] = {0};
    Cover c = {g->len, g->adj, mark, NULL, 0};
    int got = 0;
    cover_rec(&c, src);
    for (int i = 0; i < g->n; i++) {
        if (mark[i]) {
            got++;
        }
    }
    return got;
}

static void fill_weak(const Graph *g, int *wlen, int wadj[][G_MAX]) {
    for (int u = 0; u < g->n; u++) {
        for (int v = u + 1; v < g->n; v++) {
            if (g->mat[u][v] == 0 && g->mat[v][u] == 0) {
                continue;
            }
            wadj[u][wlen[u]++] = v;
            wadj[v][wlen[v]++] = u;
        }
    }
}

void walk_reach(const Graph *g, FILE *out) {
    if (g->kind == G_U) {
        int starts = adj_component_starts(g->n, g->len, g->adj);
        fprintf(out, "kind=U\ncomponents=%d\nconnected=%s\n", starts, starts == 1 ? "yes" : "no");
        return;
    }
    fprintf(out, "kind=D\ndfs-starts=%d\n", adj_component_starts(g->n, g->len, g->adj));
    int strong = 1;
    for (int s = 0; s < g->n; s++) {
        int got = reachable(g, s);
        fprintf(out, "from-%d=%d\n", s, got);
        if (got != g->n) {
            strong = 0;
        }
    }
    int wlen[G_MAX] = {0};
    int wadj[G_MAX][G_MAX];
    fill_weak(g, wlen, wadj);
    fprintf(out, "strong=%s\nweak=%s\n", strong ? "yes" : "no",
            adj_component_starts(g->n, wlen, wadj) == 1 ? "yes" : "no");
}

/* 访问和标记都在入队前。出队才标记的话，同一个顶点会被多个前驱重复放进队列。
 * 循环队列牺牲一格，cap 取 n+1 才能放下 n 个顶点。 */
static void bfs_one(const Graph *g, int src, int *mark, Tally *t, FILE *out) {
    int order[G_MAX];
    int tu[G_MAX];
    int tv[G_MAX];
    int nord = 0;
    int nt = 0;
    char ch = link_ch(g->kind);
    CycQ q;
    cyc_init(&q, g->n + 1);
    mark[src] = 1;
    order[nord++] = src;
    t->visits++;
    fprintf(out, "visit %d\n", src);
    show_enq(&q, src, out);
    while (!cyc_empty(&q)) {
        int u = 0;
        cyc_deq(&q, &u);
        fprintf(out, "deq %d queue=", u);
        print_queue(&q, out);
        fputc('\n', out);
        for (int i = 0; i < g->len[u]; i++) {
            int v = g->adj[u][i];
            t->scans++;
            fprintf(out, "scan %d%c%d %s\n", u, ch, v, mark[v] ? "seen" : "new");
            if (mark[v]) {
                continue;
            }
            mark[v] = 1;
            order[nord++] = v;
            t->visits++;
            tu[nt] = u;
            tv[nt] = v;
            nt++;
            fprintf(out, "visit %d\n", v);
            show_enq(&q, v, out);
        }
    }
    print_order(order, nord, out);
    print_tree(tu, tv, nt, ch, out);
}

static void finish_one(Tally *t, int before, FILE *out) {
    fprintf(out, "reached=%d\nscans=%d\n", t->visits - before, t->scans);
}

static void finish_all(const Graph *g, const Tally *t, FILE *out) {
    fprintf(out, "starts=%d\nvisits=%d\nscans=%d\nlist-nodes=%d\n", t->starts, t->visits, t->scans,
            graph_list_nodes(g));
}

int walk_bfs(const Graph *g, int src, FILE *out) {
    int mark[G_MAX] = {0};
    Tally t = {0, 0, 0};
    if (src < 0 || src >= g->n) {
        return -1;
    }
    fprintf(out, "bfs %d\n", src);
    bfs_one(g, src, mark, &t, out);
    finish_one(&t, 0, out);
    return 0;
}

int walk_bfs_all(const Graph *g, FILE *out) {
    int mark[G_MAX] = {0};
    Tally t = {0, 0, 0};
    fprintf(out, "bfs-all\n");
    for (int v = 0; v < g->n; v++) {
        if (mark[v]) {
            continue;
        }
        t.starts++;
        fprintf(out, "start %d\n", v);
        bfs_one(g, v, mark, &t, out);
    }
    finish_all(g, &t, out);
    return 0;
}

static void dfs_rec(WalkCtx *c, int u) {
    c->mark[u] = 1;
    c->order[c->nord++] = u;
    c->tally->visits++;
    fprintf(c->out, "enter %d\n", u);
    for (int i = 0; i < c->g->len[u]; i++) {
        int w = c->g->adj[u][i];
        c->tally->scans++;
        if (c->mark[w]) {
            continue;
        }
        c->tu[c->nt] = u;
        c->tv[c->nt] = w;
        c->nt++;
        dfs_rec(c, w);
    }
    fprintf(c->out, "back %d\n", u);
}

static void dfs_one(const Graph *g, int src, int *mark, Tally *t, FILE *out) {
    int order[G_MAX];
    int tu[G_MAX];
    int tv[G_MAX];
    WalkCtx c = {g, mark, order, 0, tu, tv, 0, t, out};
    dfs_rec(&c, src);
    print_order(order, c.nord, out);
    print_tree(tu, tv, c.nt, link_ch(g->kind), out);
}

int walk_dfs(const Graph *g, int src, FILE *out) {
    int mark[G_MAX] = {0};
    Tally t = {0, 0, 0};
    if (src < 0 || src >= g->n) {
        return -1;
    }
    fprintf(out, "dfs %d\n", src);
    dfs_one(g, src, mark, &t, out);
    finish_one(&t, 0, out);
    return 0;
}

int walk_dfs_all(const Graph *g, FILE *out) {
    int mark[G_MAX] = {0};
    Tally t = {0, 0, 0};
    fprintf(out, "dfs-all\n");
    for (int v = 0; v < g->n; v++) {
        if (mark[v]) {
            continue;
        }
        t.starts++;
        fprintf(out, "start %d\n", v);
        dfs_one(g, v, mark, &t, out);
    }
    finish_all(g, &t, out);
    return 0;
}

/* 栈里只压顶点编号。下一个邻接的位置记在游标里，不压进栈。
 * 每次只取一个未访问邻接，所以次序和递归 DFS 相同。 */
static void stack_one(const Graph *g, int src, int *mark, Tally *t, FILE *out) {
    int order[G_MAX];
    int tu[G_MAX];
    int tv[G_MAX];
    int cursor[G_MAX] = {0};
    int nord = 0;
    int nt = 0;
    char ch = link_ch(g->kind);
    SqStack s;
    sq_init(&s, g->n);
    mark[src] = 1;
    order[nord++] = src;
    t->visits++;
    fprintf(out, "visit %d\n", src);
    push_show(&s, src, out);
    while (!sq_empty(&s)) {
        int u = 0;
        int found = -1;
        sq_top(&s, &u);
        while (cursor[u] < g->len[u]) {
            int w = g->adj[u][cursor[u]++];
            t->scans++;
            if (!mark[w]) {
                found = w;
                break;
            }
        }
        if (found < 0) {
            int v = 0;
            fprintf(out, "top %d next=-\n", u);
            sq_pop(&s, &v);
            pop_show(&s, v, out);
            continue;
        }
        fprintf(out, "top %d next=%d\n", u, found);
        mark[found] = 1;
        order[nord++] = found;
        t->visits++;
        tu[nt] = u;
        tv[nt] = found;
        nt++;
        fprintf(out, "visit %d\n", found);
        push_show(&s, found, out);
    }
    print_order(order, nord, out);
    print_tree(tu, tv, nt, ch, out);
}

int walk_dfs_stack(const Graph *g, int src, FILE *out) {
    int mark[G_MAX] = {0};
    Tally t = {0, 0, 0};
    if (src < 0 || src >= g->n) {
        return -1;
    }
    fprintf(out, "dfs-stack %d\n", src);
    stack_one(g, src, mark, &t, out);
    finish_one(&t, 0, out);
    return 0;
}

/* 邻接点从左到右入栈，栈顶是最后一个。出栈才访问，兄弟的次序和递归相反。
 * 入栈时就标记，后面才被递归走到的边不会再成为树边。 */
static void flip_one(const Graph *g, int src, int *mark, Tally *t, FILE *out) {
    int order[G_MAX];
    int tu[G_MAX];
    int tv[G_MAX];
    int nord = 0;
    int nt = 0;
    char ch = link_ch(g->kind);
    SqStack s;
    sq_init(&s, g->n);
    mark[src] = 1;
    push_show(&s, src, out);
    while (!sq_empty(&s)) {
        int u = 0;
        sq_pop(&s, &u);
        fprintf(out, "pop %d visit %d stack=", u, u);
        sq_print(&s, out);
        fputc('\n', out);
        order[nord++] = u;
        t->visits++;
        for (int i = 0; i < g->len[u]; i++) {
            int w = g->adj[u][i];
            t->scans++;
            if (mark[w]) {
                continue;
            }
            mark[w] = 1;
            tu[nt] = u;
            tv[nt] = w;
            nt++;
            push_show(&s, w, out);
        }
    }
    print_order(order, nord, out);
    print_tree(tu, tv, nt, ch, out);
}

int walk_dfs_flip(const Graph *g, int src, FILE *out) {
    int mark[G_MAX] = {0};
    Tally t = {0, 0, 0};
    if (src < 0 || src >= g->n) {
        return -1;
    }
    fprintf(out, "dfs-flip %d\n", src);
    flip_one(g, src, mark, &t, out);
    finish_one(&t, 0, out);
    return 0;
}
