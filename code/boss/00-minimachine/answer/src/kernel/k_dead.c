#include "kernel/k_dead.h"

#include <stdio.h>
#include <string.h>

static int leq(const int *a, const int *b, int m) {
    int j;
    for (j = 0; j < m; j++) {
        if (a[j] > b[j]) {
            return 0;
        }
    }
    return 1;
}

static int any(const int *a, int m) {
    int j;
    for (j = 0; j < m; j++) {
        if (a[j] != 0) {
            return 1;
        }
    }
    return 0;
}

static void copy_vec(int *dst, const int *src, int m) {
    int j;
    for (j = 0; j < m; j++) {
        dst[j] = src[j];
    }
}

static void add_to(int *dst, const int *src, int m) {
    int j;
    for (j = 0; j < m; j++) {
        dst[j] += src[j];
    }
}

static void pname(char *buf, int cap, int index, int base) {
    snprintf(buf, (size_t)cap, "P%d", index + base);
}

static void vprint(FILE *fp, const int *v, int m) {
    int j;
    for (j = 0; j < m; j++) {
        fprintf(fp, "%s%d", j == 0 ? "" : ",", v[j]);
    }
}

static void names(FILE *fp, const int *ids, int n, int base) {
    int i;
    if (n <= 0) {
        fprintf(fp, "-");
        return;
    }
    for (i = 0; i < n; i++) {
        char name[16];
        pname(name, (int)sizeof name, ids[i], base);
        fprintf(fp, "%s%s", i == 0 ? "" : " ", name);
    }
}

static int isolated_row(const KRag *g, int i) {
    return !any(g->alloc[i], g->m) && !any(g->req[i], g->m);
}

static int pick_free(const KRag *g, const int *finish, const int *work) {
    int i;
    for (i = 0; i < g->n; i++) {
        if (!finish[i] && leq(g->req[i], work, g->m)) {
            return i;
        }
    }
    return -1;
}

static void push_cut(const KRag *g, KRed *out, int who, int *work) {
    int slot = out->nstep;
    copy_vec(out->step_work[slot], work, g->m);
    copy_vec(out->step_req[slot], g->req[who], g->m);
    copy_vec(out->step_alloc[slot], g->alloc[who], g->m);
    add_to(work, g->alloc[who], g->m);
    copy_vec(out->step_next[slot], work, g->m);
    out->step_who[slot] = who;
    out->removed[out->nremoved++] = who;
    out->nstep++;
}

static void link_edge(int adj[][K_BANK_N], int *len, int from, int to) {
    int i;
    if (from == to) {
        return;
    }
    for (i = 0; i < len[from]; i++) {
        if (adj[from][i] == to) {
            return;
        }
    }
    if (len[from] < K_BANK_N) {
        adj[from][len[from]++] = to;
    }
}

/* 申请超过现有可用的那一类，等着每一个还占着它的进程。这就是等待图的边。 */
static void wait_edges(const KRag *g, int adj[][K_BANK_N], int *len) {
    int i;
    int j;
    int h;
    for (i = 0; i < g->n; i++) {
        for (j = 0; j < g->m; j++) {
            if (g->req[i][j] <= g->avail[j]) {
                continue;
            }
            for (h = 0; h < g->n; h++) {
                if (h != i && g->alloc[h][j] > 0) {
                    link_edge(adj, len, i, h);
                }
            }
        }
    }
}

static int walk(int u, const int adj[][K_BANK_N], const int *len, int *color, int *path, int depth, KRed *out) {
    int i;
    color[u] = 1;
    path[depth] = u;
    for (i = 0; i < len[u]; i++) {
        int v = adj[u][i];
        int start;
        if (color[v] == 1) {
            start = 0;
            while (start <= depth && path[start] != v) {
                start++;
            }
            out->ncycle = 0;
            while (start <= depth && out->ncycle < K_BANK_N) {
                out->cyc[out->ncycle++] = path[start++];
            }
            return 1;
        }
        if (color[v] == 0 && walk(v, adj, len, color, path, depth + 1, out)) {
            return 1;
        }
    }
    color[u] = 2;
    return 0;
}

/*
 * lib/12 的 graph.c 和 lib/13 重名，不能链进来。
 * 这里按它的邻接表加递归深度优先，多一个灰色，用来抓住回到路径上的边。
 * 一类资源有多份时，有环只说明可能死锁，化简才是定论。
 */
static void find_cycle(const KRag *g, KRed *out) {
    int adj[K_BANK_N][K_BANK_N];
    int len[K_BANK_N];
    int color[K_BANK_N];
    int path[K_BANK_N];
    int i;
    memset(adj, 0, sizeof adj);
    memset(len, 0, sizeof len);
    memset(color, 0, sizeof color);
    wait_edges(g, adj, len);
    for (i = 0; i < g->n; i++) {
        if (color[i] == 0 && walk(i, adj, len, color, path, 0, out)) {
            out->cycle = 1;
            return;
        }
    }
}

int k_rag_reduce(const KRag *g, KRed *out) {
    int finish[K_BANK_N];
    int work[K_BANK_M];
    int i;
    if (g == NULL || out == NULL || g->n < 1 || g->n > K_BANK_N || g->m < 1 || g->m > K_BANK_M) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    copy_vec(work, g->avail, g->m);
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
    copy_vec(out->stuck, work, g->m);
    for (i = 0; i < g->n; i++) {
        if (!finish[i]) {
            out->left[out->nleft++] = i;
        }
    }
    out->deadlock = out->nleft > 0;
    find_cycle(g, out);
    return 0;
}

/* 手里是空的人被撤销后还不回资源，环拆不掉。代价相同取编号最小，输出才唯一。 */
static int pick_victim(const KRag *g, const KRed *t, const int *cost) {
    int i;
    int best = -1;
    for (i = 0; i < t->nleft; i++) {
        int who = t->left[i];
        if (!any(g->alloc[who], g->m)) {
            continue;
        }
        if (best < 0 || cost[who] < cost[best] || (cost[who] == cost[best] && who < best)) {
            best = who;
        }
    }
    return best;
}

static void abort_one(KRag *g, int who) {
    add_to(g->avail, g->alloc[who], g->m);
    memset(g->alloc[who], 0, sizeof g->alloc[who]);
    memset(g->req[who], 0, sizeof g->req[who]);
}

int k_rag_recover(const KRag *g, const int *cost, KRecover *out) {
    KRag cur;
    int round;
    if (g == NULL || cost == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    cur = *g;
    for (round = 0; round <= g->n; round++) {
        KRed red;
        int who;
        if (k_rag_reduce(&cur, &red) != 0) {
            return -1;
        }
        if (!red.deadlock) {
            out->recovered = 1;
            return 0;
        }
        who = pick_victim(&cur, &red, cost);
        if (who < 0 || out->naborts >= K_BANK_N) {
            return 0;
        }
        out->who[out->naborts] = who;
        out->cost[out->naborts] = cost[who];
        abort_one(&cur, who);
        copy_vec(out->avail[out->naborts], cur.avail, g->m);
        out->naborts++;
    }
    return 0;
}

int k_bound_list(const int *k, int n, long long *least) {
    int i;
    long long sum = 0;
    if (k == NULL || least == NULL || n < 1) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (k[i] < 1) {
            return -1;
        }
        sum += (long long)k[i] - 1;
    }
    *least = sum + 1;
    return 0;
}

int k_bound_equal(int n, int k, long long *least) {
    if (n < 1 || k < 1 || least == NULL) {
        return -1;
    }
    *least = (long long)n * (k - 1) + 1;
    return 0;
}

int k_bound_mink(int resources, int each, int *out) {
    if (resources < 1 || each < 2 || out == NULL) {
        return -1;
    }
    *out = (resources - 1) / (each - 1) + 1;
    return 0;
}

const char *k_claim(const char *name) {
    static const struct {
        const char *name;
        const char *truth;
    } rows[] = {
        {"s1-limit-order", "false"},
        {"s1-need-max", "true"},
        {"s1-refuse-unsafe", "true"},
        {"deprive-recover", "true"},
        {"prevent-ensures", "true"},
        {"banker-detects", "false"},
        {"deadlock-two-block", "true"},
        {"unsafe-is-dead", "false"},
        {"cycle-many-enough", "false"},
    };
    int i;
    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        if (strcmp(rows[i].name, name) == 0) {
            return rows[i].truth;
        }
    }
    return NULL;
}

void k_dead_print(const KRag *g, const KRed *out, FILE *fp) {
    int i;
    fprintf(fp, "algo=reduce deadlock=%s cycle=%s\n", out->deadlock ? "yes" : "no", out->cycle ? "yes" : "no");
    fprintf(fp, "isolated=");
    names(fp, out->isolated, out->niso, g->base);
    fprintf(fp, "\n");
    for (i = 0; i < out->nstep; i++) {
        char name[16];
        pname(name, (int)sizeof name, out->step_who[i], g->base);
        fprintf(fp, "%s work=", name);
        vprint(fp, out->step_work[i], g->m);
        fprintf(fp, " request=");
        vprint(fp, out->step_req[i], g->m);
        fprintf(fp, " alloc=");
        vprint(fp, out->step_alloc[i], g->m);
        fprintf(fp, " next=");
        vprint(fp, out->step_next[i], g->m);
        fprintf(fp, "\n");
    }
    if (out->deadlock) {
        fprintf(fp, "stuck=");
        vprint(fp, out->stuck, g->m);
        fprintf(fp, "\n");
    }
    if (out->cycle) {
        fprintf(fp, "cycle=");
        names(fp, out->cyc, out->ncycle, g->base);
        fprintf(fp, "\n");
    }
    fprintf(fp, "removed=");
    names(fp, out->removed, out->nremoved, g->base);
    fprintf(fp, "\nleft=");
    names(fp, out->left, out->nleft, g->base);
    fprintf(fp, "\n");
}

void k_recover_print(const KRag *g, const KRecover *out, FILE *fp) {
    int i;
    fprintf(fp, "algo=recover recovered=%s\n", out->recovered ? "yes" : "no");
    if (out->naborts == 0) {
        fprintf(fp, "abort=-\n");
        return;
    }
    for (i = 0; i < out->naborts; i++) {
        char name[16];
        pname(name, (int)sizeof name, out->who[i], g->base);
        fprintf(fp, "abort=%s cost=%d avail=", name, out->cost[i]);
        vprint(fp, out->avail[i], g->m);
        fprintf(fp, "\n");
    }
}
