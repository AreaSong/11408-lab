#include "run_sdn.h"

#include "scan.h"
#include "sdn.h"

#include <stdio.h>
#include <string.h>

static Sdn sdn;

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

static void print_rule(int sw, int host) {
    int nxt = sdn_next(&sdn, sw, host);

    if (nxt == SDN_DELIVER) {
        printf("%s 匹配 %s 动作 交付\n", sdn.topo.name[sw], sdn.hname[host]);
        return;
    }
    if (nxt < 0) {
        printf("%s 匹配 %s 动作 丢弃\n", sdn.topo.name[sw], sdn.hname[host]);
        return;
    }
    printf("%s 匹配 %s 动作 输出端口 %d（到 %s）\n", sdn.topo.name[sw], sdn.hname[host], sdn.port[sw][nxt],
           sdn.topo.name[nxt]);
}

static void action_text(char *buf, int cap, int sw, int nxt) {
    if (nxt == SDN_DELIVER) {
        snprintf(buf, (size_t)cap, "交付");
    } else if (nxt < 0) {
        snprintf(buf, (size_t)cap, "丢弃");
    } else {
        snprintf(buf, (size_t)cap, "输出端口 %d（到 %s）", sdn.port[sw][nxt], sdn.topo.name[nxt]);
    }
}

static int cmd_init(char *p) {
    if (need_end(p) != 0) {
        return -1;
    }
    sdn_init(&sdn);
    return 0;
}

static int cmd_switch(char *p) {
    char name[TOPO_NAME];
    char err[80];

    if (take_name(&p, name, (int)sizeof name, "缺少交换机名") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (sdn_add_switch(&sdn, name, err, (int)sizeof err) < 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_host(char *p) {
    char host[TOPO_NAME];
    char sw[TOPO_NAME];
    char err[80];

    if (take_name(&p, host, (int)sizeof host, "缺少主机名") != 0 ||
        take_name(&p, sw, (int)sizeof sw, "缺少交换机名") != 0 || need_end(p) != 0) {
        return -1;
    }
    if (sdn_add_host(&sdn, host, sw, err, (int)sizeof err) < 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_link(char *p) {
    char a[TOPO_NAME];
    char b[TOPO_NAME];
    char err[80];
    int cost = 0;

    if (take_name(&p, a, (int)sizeof a, "缺少交换机") != 0 ||
        take_name(&p, b, (int)sizeof b, "缺少交换机") != 0 || take_int(&p, &cost) != 1 || need_end(p) != 0) {
        return reject("link 要写两台交换机和正整数代价");
    }
    if (sdn_link(&sdn, a, b, cost, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_install(char *p) {
    char err[80];

    if (need_end(p) != 0) {
        return -1;
    }
    if (sdn_install(&sdn, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    printf("控制平面：控制器掌握全网拓扑，用 Dijkstra 计算路径，经南向接口下发流表。\n");
    printf("数据平面：交换机只查流表做匹配-动作，不互相交换路由。\n");
    for (int sw = 0; sw < sdn.topo.n; sw++) {
        for (int h = 0; h < sdn.nh; h++) {
            print_rule(sw, h);
        }
    }
    return 0;
}

static int cmd_fail(char *p) {
    char a[TOPO_NAME];
    char b[TOPO_NAME];
    char err[80];
    char was[64];
    char now[64];
    int old[G_NMAX][SDN_HMAX];

    if (!sdn.ready) {
        return reject("要先 install");
    }
    if (take_name(&p, a, (int)sizeof a, "缺少交换机") != 0 ||
        take_name(&p, b, (int)sizeof b, "缺少交换机") != 0 || need_end(p) != 0) {
        return -1;
    }
    memcpy(old, sdn.flow, sizeof old);
    if (sdn_fail(&sdn, a, b, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    printf("控制平面发现 %s—%s 断开，重新计算并下发。\n", a, b);
    for (int sw = 0; sw < sdn.topo.n; sw++) {
        for (int h = 0; h < sdn.nh; h++) {
            if (old[sw][h] == sdn.flow[sw][h]) {
                continue;
            }
            action_text(was, (int)sizeof was, sw, old[sw][h]);
            action_text(now, (int)sizeof now, sw, sdn.flow[sw][h]);
            printf("%s 到 %s 的动作由%s改为%s\n", sdn.topo.name[sw], sdn.hname[h], was, now);
        }
    }
    return 0;
}

static int cmd_go(char *p) {
    char src[TOPO_NAME];
    char dst[TOPO_NAME];
    int sh;
    int dh;
    int at;
    int guard = 0;

    if (!sdn.ready) {
        return reject("要先 install");
    }
    if (take_name(&p, src, (int)sizeof src, "缺少源主机") != 0 ||
        take_name(&p, dst, (int)sizeof dst, "缺少目的主机") != 0 || need_end(p) != 0) {
        return -1;
    }
    sh = sdn_find_host(&sdn, src);
    dh = sdn_find_host(&sdn, dst);
    if (sh < 0 || dh < 0) {
        return reject("没有这台主机");
    }
    printf("数据平面从 %s 到 %s\n", src, dst);
    at = sdn.home[sh];
    while (at != sdn.home[dh]) {
        int nxt = sdn_next(&sdn, at, dh);

        if (nxt < 0) {
            printf("%s 匹配 %s 动作 丢弃\n", sdn.topo.name[at], dst);
            return 0;
        }
        printf("%s 匹配 %s 动作 输出端口 %d（到 %s）\n", sdn.topo.name[at], dst, sdn.port[at][nxt],
               sdn.topo.name[nxt]);
        at = nxt;
        if (++guard > sdn.topo.n) {
            printf("环路\n");
            return 0;
        }
    }
    printf("%s 匹配 %s 动作 交付\n", sdn.topo.name[at], dst);
    return 0;
}

static int cmd_south(char *p) {
    if (need_end(p) != 0) {
        return -1;
    }
    printf("SDN 控制器向数据平面的交换机下发流表，用的是南向接口。\n");
    printf("北向接口给控制器上方的应用，东西向接口用在多台控制器之间。\n");
    printf("结果：南向接口\n");
    printf("选项 B\n");
    return 0;
}

int run_sdn(char *line) {
    char cmd[16];
    char *p = line;
    int rc = take_token(&p, cmd, (int)sizeof cmd);

    if (rc != 1) {
        return reject(rc < 0 ? "命令太长" : "空命令");
    }
    if (strcmp(cmd, "init") == 0) {
        return cmd_init(p);
    }
    if (strcmp(cmd, "switch") == 0) {
        return cmd_switch(p);
    }
    if (strcmp(cmd, "host") == 0) {
        return cmd_host(p);
    }
    if (strcmp(cmd, "link") == 0) {
        return cmd_link(p);
    }
    if (strcmp(cmd, "install") == 0) {
        return cmd_install(p);
    }
    if (strcmp(cmd, "fail") == 0) {
        return cmd_fail(p);
    }
    if (strcmp(cmd, "go") == 0) {
        return cmd_go(p);
    }
    if (strcmp(cmd, "south") == 0) {
        return cmd_south(p);
    }
    return reject("没有这条命令");
}
