#ifndef RIP_H
#define RIP_H

/*
 * 距离按王道记：直连网络是 1，16 表示不可达，不是第 16 跳。
 * 邻居通告的是它自己的距离，本机使用前先加 1，到 16 就封顶。
 * 原下一跳的新距离无论变大变小都采纳；其他邻居只有严格更短才换。
 * 距离相等不换下一跳。
 */

#define RIP_RMAX 8
#define RIP_NMAX 8
#define RIP_NAME 32
#define RIP_INF 16
#define RIP_NONE -1
#define RIP_DIRECT -2

typedef struct {
    int metric;
    int via;
    int offer[RIP_RMAX];
    int has[RIP_RMAX];
} RipEntry;

typedef struct {
    int nr;
    int nn;
    int split;
    char rname[RIP_RMAX][RIP_NAME];
    char nname[RIP_NMAX][RIP_NAME];
    int adj[RIP_RMAX][RIP_RMAX];
    int alive[RIP_RMAX][RIP_NMAX];
    RipEntry e[RIP_RMAX][RIP_NMAX];
} Rip;

typedef struct {
    int t;
    int router;
    int metric;
    int via;
} RipHit;

typedef struct {
    int round;
    int from;
    int to;
    int suppressed;
    int metric[RIP_RMAX];
    int via[RIP_RMAX];
} SlowStep;

void rip_init(Rip *r);
int rip_find_router(const Rip *r, const char *name);
int rip_find_net(const Rip *r, const char *name);
int rip_add_router(Rip *r, const char *name, char *err, int cap);
int rip_add_net(Rip *r, const char *name, char *err, int cap);
int rip_rlink(Rip *r, int a, int b, char *err, int cap);
int rip_nlink(Rip *r, int router, int net, char *err, int cap);
int rip_set(Rip *r, int router, int net, int metric, int via, char *err, int cap);
int rip_learn(Rip *r, int router, int from, int net, int metric, char *err, int cap);
int rip_down(Rip *r, int router, int net, char *err, int cap);
int rip_hear(Rip *r, int router, int from, char *err, int cap);
int rip_relax(Rip *r);
int rip_metric(const Rip *r, int router, int net);
int rip_via(const Rip *r, int router, int net);
const char *rip_hop_name(const Rip *r, int via);
const char *rip_pick_proto(int routers);
int rip_min_ttl(int routers);
int rip_spread(const Rip *src, int net, int period, RipHit *hit, int cap, int *nhit, int *seconds);
int rip_slow(Rip *r, int failed, int net, SlowStep *steps, int cap, int *nstep, char *err, int errcap);

#endif
