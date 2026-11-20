#include "run_rip.h"

#include "rip.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

static Rip rip;

static int need_end(const char *p) {
    if (!rest_blank(p)) {
        return reject("多了参数");
    }
    return 0;
}

static int take_name(char **p, char *out, int cap, const char *what) {
    int rc = take_token(p, out, cap);

    if (rc != 1) {
        return reject(rc < 0 ? "参数太长" : what);
    }
    return 0;
}

static int cmd_init(char *p) {
    if (need_end(p) != 0) {
        return -1;
    }
    rip_init(&rip);
    return 0;
}

static int cmd_router(char *p) {
    char name[RIP_NAME];
    char err[80];

    if (take_name(&p, name, (int)sizeof name, "缺少路由器名") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (rip_add_router(&rip, name, err, (int)sizeof err) < 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_net(char *p) {
    char name[RIP_NAME];
    char err[80];

    if (take_name(&p, name, (int)sizeof name, "缺少网络名") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (rip_add_net(&rip, name, err, (int)sizeof err) < 0) {
        return reject(err);
    }
    return 0;
}

static int pair_ids(char **p, int *a, int *b, int nets) {
    char x[RIP_NAME];
    char y[RIP_NAME];

    if (take_name(p, x, (int)sizeof x, "缺少名字") != 0 ||
        take_name(p, y, (int)sizeof y, "缺少名字") != 0) {
        return -1;
    }
    *a = rip_find_router(&rip, x);
    *b = nets ? rip_find_net(&rip, y) : rip_find_router(&rip, y);
    if (*a < 0 || *b < 0) {
        return reject(nets ? "没有这台路由器或这个网络" : "没有这两台路由器");
    }
    return 0;
}

static int cmd_rlink(char *p) {
    char err[80];
    int a = 0;
    int b = 0;

    if (pair_ids(&p, &a, &b, 0) != 0 || need_end(p) != 0) {
        return -1;
    }
    if (rip_rlink(&rip, a, b, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_nlink(char *p) {
    char err[80];
    int a = 0;
    int b = 0;

    if (pair_ids(&p, &a, &b, 1) != 0 || need_end(p) != 0) {
        return -1;
    }
    if (rip_nlink(&rip, a, b, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_split(char *p) {
    char flag[8];

    if (take_name(&p, flag, (int)sizeof flag, "split 后面写 on 或 off") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (strcmp(flag, "on") == 0) {
        rip.split = 1;
    } else if (strcmp(flag, "off") == 0) {
        rip.split = 0;
    } else {
        return reject("split 后面写 on 或 off");
    }
    return 0;
}

static int cmd_set(char *p) {
    char via_name[RIP_NAME];
    char err[80];
    int router = 0;
    int net = 0;
    int metric = 0;
    int via;

    if (pair_ids(&p, &router, &net, 1) != 0) {
        return -1;
    }
    if (take_int(&p, &metric) != 1) {
        return reject("set 要写路由器、网络和距离");
    }
    if (take_name(&p, via_name, (int)sizeof via_name, "缺少下一跳") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (strcmp(via_name, "direct") == 0) {
        via = RIP_DIRECT;
    } else {
        via = rip_find_router(&rip, via_name);
        if (via < 0) {
            return reject("没有这台下一跳");
        }
    }
    if (rip_set(&rip, router, net, metric, via, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_learn(char *p) {
    char net_name[RIP_NAME];
    char err[80];
    int router = 0;
    int from = 0;
    int net;
    int metric = 0;

    if (pair_ids(&p, &router, &from, 0) != 0) {
        return -1;
    }
    if (take_name(&p, net_name, (int)sizeof net_name, "缺少网络") != 0) {
        return -1;
    }
    if (take_int(&p, &metric) != 1 || need_end(p) != 0) {
        return reject("learn 要写本机、邻居、网络和邻居自己的距离");
    }
    net = rip_find_net(&rip, net_name);
    if (net < 0) {
        return reject("没有这个网络");
    }
    if (rip_learn(&rip, router, from, net, metric, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_down(char *p) {
    char err[80];
    int router = 0;
    int net = 0;

    if (pair_ids(&p, &router, &net, 1) != 0 || need_end(p) != 0) {
        return -1;
    }
    if (rip_down(&rip, router, net, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_hear(char *p) {
    char err[80];
    int router = 0;
    int from = 0;

    if (pair_ids(&p, &router, &from, 0) != 0 || need_end(p) != 0) {
        return -1;
    }
    if (rip_hear(&rip, router, from, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    return 0;
}

static void print_route(int router, int net) {
    printf("%s %s %d %s\n", rip.rname[router], rip.nname[net], rip_metric(&rip, router, net),
           rip_hop_name(&rip, rip_via(&rip, router, net)));
}

static int cmd_tables(char *p) {
    char name[RIP_NAME];
    int only = -1;
    int rc = take_token(&p, name, (int)sizeof name);

    if (rc == 1) {
        only = rip_find_router(&rip, name);
        if (only < 0) {
            return reject("没有这台路由器");
        }
    } else if (rc < 0) {
        return reject("参数太长");
    }
    if (need_end(p) != 0) {
        return -1;
    }
    for (int u = 0; u < rip.nr; u++) {
        if (only >= 0 && u != only) {
            continue;
        }
        for (int d = 0; d < rip.nn; d++) {
            print_route(u, d);
        }
    }
    return 0;
}

static int cmd_show(char *p) {
    int router = 0;
    int net = 0;

    if (pair_ids(&p, &router, &net, 1) != 0 || need_end(p) != 0) {
        return -1;
    }
    print_route(router, net);
    return 0;
}

static int cmd_as(char *p) {
    int n = 0;

    if (take_int(&p, &n) != 1 || n < 1 || need_end(p) != 0) {
        return reject("as 后面写可能经过的路由器数");
    }
    if (n > 15) {
        printf("经过的路由器有 %d 台，RIP 距离会到 16，选 OSPF\n", n);
    } else {
        printf("经过的路由器有 %d 台，RIP 的最大距离仍够用，选 RIP\n", n);
    }
    printf("结果：%s\n", rip_pick_proto(n));
    return 0;
}

static int cmd_ttl(char *p) {
    int n = 0;

    if (take_int(&p, &n) != 1 || n < 1 || need_end(p) != 0) {
        return reject("ttl 后面写可能经过的路由器数");
    }
    printf("经过的路由器有 %d 台，初始 TTL 至少 %d\n", n, rip_min_ttl(n));
    printf("结果：%d\n", rip_min_ttl(n));
    return 0;
}

static int cmd_spread(char *p) {
    char name[RIP_NAME];
    RipHit hit[RIP_RMAX * 2];
    int period = 0;
    int net;
    int n = 0;
    int sec = 0;

    if (take_name(&p, name, (int)sizeof name, "缺少网络") != 0 || take_int(&p, &period) != 1 ||
        need_end(p) != 0) {
        return reject("spread 要写网络和周期秒数");
    }
    net = rip_find_net(&rip, name);
    if (net < 0) {
        return reject("没有这个网络");
    }
    if (rip_spread(&rip, net, period, hit, (int)(sizeof hit / sizeof hit[0]), &n, &sec) != 0) {
        return reject("距离没有在 16 以内传遍");
    }
    printf("【%s】周期 %ds\n", name, period);
    for (int i = 0; i < n; i++) {
        const char *hop = rip_hop_name(&rip, hit[i].via);

        if (hit[i].t < 0) {
            printf("一开始 %s %d %s\n", rip.rname[hit[i].router], hit[i].metric, hop);
        } else {
            printf("t=%ds %s %d %s\n", hit[i].t, rip.rname[hit[i].router], hit[i].metric, hop);
        }
    }
    printf("结果：%ds\n", sec);
    return 0;
}

static int cmd_slow(char *p) {
    char err[96];
    SlowStep step[24];
    int router = 0;
    int net = 0;
    int n = 0;

    if (pair_ids(&p, &router, &net, 1) != 0 || need_end(p) != 0) {
        return -1;
    }
    if (rip_slow(&rip, router, net, step, 24, &n, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    for (int i = 0; i < n; i++) {
        if (step[i].round == 0) {
            printf("轮 0\n");
        } else if (step[i].suppressed) {
            printf("轮 %d %s 到 %s 水平分割\n", step[i].round, rip.rname[step[i].from],
                   rip.rname[step[i].to]);
        } else {
            printf("轮 %d %s 到 %s\n", step[i].round, rip.rname[step[i].from], rip.rname[step[i].to]);
        }
        for (int u = 0; u < rip.nr; u++) {
            printf("%s %s %d %s\n", rip.rname[u], rip.nname[net], step[i].metric[u],
                   rip_hop_name(&rip, step[i].via[u]));
        }
    }
    printf("收敛 %d\n", n - 1);
    return 0;
}

static int cmd_converge(char *p) {
    if (need_end(p) != 0) {
        return -1;
    }
    rip_relax(&rip);
    return 0;
}

int run_rip(char *line) {
    static const char *const name[] = {
        "init",  "router", "net",   "rlink", "nlink", "split", "set",    "learn", "converge",
        "down",  "hear",   "tables", "show", "as",    "ttl",   "spread", "slow",
    };
    static int (*const fn[])(char *) = {
        cmd_init,  cmd_router, cmd_net,   cmd_rlink, cmd_nlink, cmd_split, cmd_set,    cmd_learn,
        cmd_converge, cmd_down, cmd_hear, cmd_tables, cmd_show, cmd_as,    cmd_ttl,   cmd_spread,
        cmd_slow,
    };
    char cmd[16];
    char *p = line;
    int rc = take_token(&p, cmd, (int)sizeof cmd);

    if (rc != 1) {
        return reject(rc < 0 ? "命令太长" : "空命令");
    }
    for (int i = 0; i < (int)(sizeof name / sizeof name[0]); i++) {
        if (strcmp(cmd, name[i]) == 0) {
            return fn[i](p);
        }
    }
    return reject("没有这条命令");
}
