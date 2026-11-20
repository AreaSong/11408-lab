#include "rip.h"

#include <stdio.h>
#include <string.h>

static int bad(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return -1;
}

static int name_ok(const char *name) {
    int n = (int)strlen(name);
    return n > 0 && n < RIP_NAME;
}

static void clear_entry(RipEntry *e) {
    memset(e, 0, sizeof *e);
    e->metric = RIP_INF;
    e->via = RIP_NONE;
}

void rip_init(Rip *r) {
    memset(r, 0, sizeof *r);
}

int rip_find_router(const Rip *r, const char *name) {
    for (int i = 0; i < r->nr; i++) {
        if (strcmp(r->rname[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int rip_find_net(const Rip *r, const char *name) {
    for (int i = 0; i < r->nn; i++) {
        if (strcmp(r->nname[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int rip_add_router(Rip *r, const char *name, char *err, int cap) {
    int id;

    if (!name_ok(name)) {
        return bad(err, cap, "路由器名是空的或太长");
    }
    if (rip_find_router(r, name) >= 0) {
        return bad(err, cap, "路由器名重复");
    }
    if (r->nr >= RIP_RMAX) {
        return bad(err, cap, "路由器太多");
    }
    id = r->nr++;
    snprintf(r->rname[id], sizeof r->rname[id], "%s", name);
    for (int d = 0; d < r->nn; d++) {
        clear_entry(&r->e[id][d]);
    }
    return id;
}

int rip_add_net(Rip *r, const char *name, char *err, int cap) {
    int id;

    if (!name_ok(name)) {
        return bad(err, cap, "网络名是空的或太长");
    }
    if (rip_find_net(r, name) >= 0) {
        return bad(err, cap, "网络名重复");
    }
    if (r->nn >= RIP_NMAX) {
        return bad(err, cap, "网络太多");
    }
    id = r->nn++;
    snprintf(r->nname[id], sizeof r->nname[id], "%s", name);
    for (int u = 0; u < r->nr; u++) {
        clear_entry(&r->e[u][id]);
    }
    return id;
}

int rip_rlink(Rip *r, int a, int b, char *err, int cap) {
    if (a < 0 || b < 0 || a >= r->nr || b >= r->nr || a == b) {
        return bad(err, cap, "路由器链路的两端不对");
    }
    if (r->adj[a][b]) {
        return bad(err, cap, "这两台路由器已经相邻");
    }
    r->adj[a][b] = 1;
    r->adj[b][a] = 1;
    return 0;
}

int rip_nlink(Rip *r, int router, int net, char *err, int cap) {
    if (router < 0 || net < 0 || router >= r->nr || net >= r->nn) {
        return bad(err, cap, "直连的路由器或网络不存在");
    }
    if (r->alive[router][net]) {
        return bad(err, cap, "已经直连这个网络");
    }
    r->alive[router][net] = 1;
    r->e[router][net].metric = 1;
    r->e[router][net].via = RIP_DIRECT;
    return 0;
}

static int hop_of(int advertised) {
    if (advertised < 0 || advertised >= RIP_INF) {
        return RIP_INF;
    }
    return advertised + 1;
}

static void recompute(Rip *r, int u, int d) {
    RipEntry *e = &r->e[u][d];
    int prev = e->via;
    int best_m = RIP_INF;
    int best_via = RIP_NONE;

    if (r->alive[u][d]) {
        best_m = 1;
        best_via = RIP_DIRECT;
    }
    for (int v = 0; v < r->nr; v++) {
        int cand;

        if (!r->adj[u][v] || !e->has[v]) {
            continue;
        }
        cand = hop_of(e->offer[v]);
        if (cand < best_m) {
            best_m = cand;
            best_via = v;
        }
    }
    /* 相等就留原来的下一跳，两条一样长的路不来回换。 */
    if (prev >= 0 && r->adj[u][prev] && e->has[prev] && hop_of(e->offer[prev]) == best_m &&
        best_m < RIP_INF) {
        best_via = prev;
    }
    if (best_m >= RIP_INF) {
        best_m = RIP_INF;
        best_via = RIP_NONE;
    }
    e->metric = best_m;
    e->via = best_via;
}

int rip_set(Rip *r, int router, int net, int metric, int via, char *err, int cap) {
    RipEntry *e;

    if (router < 0 || net < 0 || router >= r->nr || net >= r->nn) {
        return bad(err, cap, "路由器或网络不存在");
    }
    e = &r->e[router][net];
    if (via == RIP_DIRECT) {
        if (metric != 1) {
            return bad(err, cap, "直连距离按王道记 1");
        }
        r->alive[router][net] = 1;
        e->metric = 1;
        e->via = RIP_DIRECT;
        return 0;
    }
    if (via < 0 || via >= r->nr || !r->adj[router][via]) {
        return bad(err, cap, "下一跳不是邻居");
    }
    if (metric < 1 || metric > RIP_INF) {
        return bad(err, cap, "距离要在 1 到 16 之间");
    }
    r->alive[router][net] = 0;
    e->metric = metric;
    e->via = metric >= RIP_INF ? RIP_NONE : via;
    e->has[via] = 1;
    e->offer[via] = metric >= RIP_INF ? RIP_INF : metric - 1;
    return 0;
}

int rip_learn(Rip *r, int router, int from, int net, int metric, char *err, int cap) {
    if (router < 0 || from < 0 || net < 0 || router >= r->nr || from >= r->nr || net >= r->nn) {
        return bad(err, cap, "路由器或网络不存在");
    }
    if (!r->adj[router][from]) {
        return bad(err, cap, "只从邻居收距离向量");
    }
    if (metric < 0 || metric > RIP_INF) {
        return bad(err, cap, "邻居通告的距离要在 0 到 16 之间");
    }
    r->e[router][net].has[from] = 1;
    r->e[router][net].offer[from] = metric;
    recompute(r, router, net);
    return 0;
}

int rip_down(Rip *r, int router, int net, char *err, int cap) {
    if (router < 0 || net < 0 || router >= r->nr || net >= r->nn) {
        return bad(err, cap, "路由器或网络不存在");
    }
    if (!r->alive[router][net]) {
        return bad(err, cap, "这条不是该路由器的直连网络");
    }
    /* 先记成 16，先别用邻居上一轮的旧通告。等下一条报文到了再重算。 */
    r->alive[router][net] = 0;
    r->e[router][net].metric = RIP_INF;
    r->e[router][net].via = RIP_NONE;
    return 0;
}

int rip_hear(Rip *r, int router, int from, char *err, int cap) {
    if (router < 0 || from < 0 || router >= r->nr || from >= r->nr || router == from) {
        return bad(err, cap, "收报文的两端不对");
    }
    if (!r->adj[router][from]) {
        return bad(err, cap, "只从邻居收距离向量");
    }
    for (int d = 0; d < r->nn; d++) {
        int advertised = r->e[from][d].metric;

        if (r->split && r->e[from][d].via == router) {
            advertised = RIP_INF;
        }
        r->e[router][d].has[from] = 1;
        r->e[router][d].offer[from] = advertised;
        recompute(r, router, d);
    }
    return 0;
}

static void rip_round(Rip *r) {
    int metric[RIP_RMAX][RIP_NMAX];
    int via[RIP_RMAX][RIP_NMAX];

    for (int u = 0; u < r->nr; u++) {
        for (int d = 0; d < r->nn; d++) {
            metric[u][d] = r->e[u][d].metric;
            via[u][d] = r->e[u][d].via;
        }
    }
    /* 同一轮用回合开始时的表。本轮新学到的距离，下一轮才继续往外传。 */
    for (int u = 0; u < r->nr; u++) {
        for (int v = 0; v < r->nr; v++) {
            if (!r->adj[u][v]) {
                continue;
            }
            for (int d = 0; d < r->nn; d++) {
                int advertised = metric[v][d];

                if (r->split && via[v][d] == u) {
                    advertised = RIP_INF;
                }
                r->e[u][d].offer[v] = advertised;
                r->e[u][d].has[v] = 1;
            }
        }
    }
    for (int u = 0; u < r->nr; u++) {
        for (int d = 0; d < r->nn; d++) {
            recompute(r, u, d);
        }
    }
}

static int table_same(const Rip *a, const Rip *b) {
    for (int u = 0; u < a->nr; u++) {
        for (int d = 0; d < a->nn; d++) {
            if (a->e[u][d].metric != b->e[u][d].metric || a->e[u][d].via != b->e[u][d].via) {
                return 0;
            }
        }
    }
    return 1;
}

int rip_relax(Rip *r) {
    int n = 0;

    while (n < 64) {
        Rip old = *r;

        rip_round(r);
        if (table_same(&old, r)) {
            break;
        }
        n++;
    }
    return n;
}

int rip_metric(const Rip *r, int router, int net) {
    return r->e[router][net].metric;
}

int rip_via(const Rip *r, int router, int net) {
    return r->e[router][net].via;
}

const char *rip_hop_name(const Rip *r, int via) {
    if (via == RIP_DIRECT) {
        return "直连";
    }
    if (via < 0 || via >= r->nr) {
        return "-";
    }
    return r->rname[via];
}

const char *rip_pick_proto(int routers) {
    /* 路径上超过 15 台路由器时，距离至少是 16，RIP 视为不可达。 */
    return routers > 15 ? "OSPF" : "RIP";
}

int rip_min_ttl(int routers) {
    return routers + 1;
}

static void rip_seed(Rip *r) {
    for (int u = 0; u < r->nr; u++) {
        for (int d = 0; d < r->nn; d++) {
            int alive = r->alive[u][d];

            clear_entry(&r->e[u][d]);
            if (alive) {
                r->e[u][d].metric = 1;
                r->e[u][d].via = RIP_DIRECT;
            }
        }
    }
}

static void rip_bfs(const Rip *r, int net, int *dist) {
    int q[RIP_RMAX];
    int qh = 0;
    int qt = 0;

    for (int u = 0; u < r->nr; u++) {
        dist[u] = RIP_INF;
    }
    for (int u = 0; u < r->nr; u++) {
        if (!r->alive[u][net]) {
            continue;
        }
        dist[u] = 1;
        q[qt++] = u;
    }
    while (qh < qt) {
        int u = q[qh++];

        for (int v = 0; v < r->nr; v++) {
            int nd;

            if (!r->adj[u][v] || dist[v] < RIP_INF) {
                continue;
            }
            nd = dist[u] + 1;
            if (nd >= RIP_INF) {
                continue;
            }
            dist[v] = nd;
            q[qt++] = v;
        }
    }
}

static int all_seen(const int *seen, int n) {
    for (int i = 0; i < n; i++) {
        if (!seen[i]) {
            return 0;
        }
    }
    return 1;
}

static void push_hit(RipHit *hit, int cap, int *count, int t, int router, int metric, int via) {
    if (*count < cap) {
        hit[*count].t = t;
        hit[*count].router = router;
        hit[*count].metric = metric;
        hit[*count].via = via;
    }
    (*count)++;
}

int rip_spread(const Rip *src, int net, int period, RipHit *hit, int cap, int *nhit, int *seconds) {
    Rip sim;
    int bfs[RIP_RMAX];
    int seen[RIP_RMAX] = {0};
    int count = 0;
    int last = 0;

    if (src == NULL || net < 0 || net >= src->nn || period <= 0) {
        return -1;
    }
    sim = *src;
    rip_seed(&sim);
    rip_bfs(&sim, net, bfs);
    for (int u = 0; u < sim.nr; u++) {
        if (sim.e[u][net].metric == bfs[u]) {
            seen[u] = 1;
            push_hit(hit, cap, &count, -1, u, sim.e[u][net].metric, sim.e[u][net].via);
        }
    }
    for (int step = 0; step < 64 && !all_seen(seen, sim.nr); step++) {
        int t = step * period;

        rip_round(&sim);
        for (int u = 0; u < sim.nr; u++) {
            if (seen[u] || sim.e[u][net].metric != bfs[u]) {
                continue;
            }
            seen[u] = 1;
            last = t;
            push_hit(hit, cap, &count, t, u, sim.e[u][net].metric, sim.e[u][net].via);
        }
    }
    if (!all_seen(seen, sim.nr) || count > cap) {
        return -1;
    }
    *nhit = count;
    *seconds = last;
    return 0;
}

static void save_step(SlowStep *s, int round, int from, int to, int suppressed, const Rip *r, int net) {
    s->round = round;
    s->from = from;
    s->to = to;
    s->suppressed = suppressed;
    for (int u = 0; u < RIP_RMAX; u++) {
        s->metric[u] = RIP_INF;
        s->via[u] = RIP_NONE;
    }
    for (int u = 0; u < r->nr; u++) {
        s->metric[u] = r->e[u][net].metric;
        s->via[u] = r->e[u][net].via;
    }
}

static int net_all_inf(const Rip *r, int net) {
    for (int u = 0; u < r->nr; u++) {
        if (r->e[u][net].metric < RIP_INF) {
            return 0;
        }
    }
    return 1;
}

int rip_slow(Rip *r, int failed, int net, SlowStep *steps, int cap, int *nstep, char *err, int errcap) {
    int other;
    int quiet = 0;
    int n = 0;

    if (r->nr != 2) {
        return bad(err, errcap, "坏消息的逐跳演示只放两台路由器");
    }
    if (failed < 0 || failed >= r->nr || net < 0 || net >= r->nn) {
        return bad(err, errcap, "路由器或网络不存在");
    }
    if (cap < 2) {
        return bad(err, errcap, "步数缓冲区太小");
    }
    if (rip_down(r, failed, net, err, errcap) != 0) {
        return -1;
    }
    other = failed == 0 ? 1 : 0;
    save_step(&steps[0], 0, -1, -1, 0, r, net);
    while (n + 1 < cap && quiet < 2 && !net_all_inf(r, net)) {
        int src = n % 2 == 0 ? other : failed;
        int dst = n % 2 == 0 ? failed : other;
        int suppressed = r->split && r->e[src][net].via == dst;
        int old_m = r->e[dst][net].metric;
        int old_v = r->e[dst][net].via;

        if (rip_hear(r, dst, src, err, errcap) != 0) {
            return -1;
        }
        n++;
        save_step(&steps[n], n, src, dst, suppressed, r, net);
        if (old_m == r->e[dst][net].metric && old_v == r->e[dst][net].via) {
            quiet++;
        } else {
            quiet = 0;
        }
    }
    *nstep = n + 1;
    return 0;
}
