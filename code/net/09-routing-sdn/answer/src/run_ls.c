#include "run_ls.h"

#include "ls.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

static Topo net;

static int need_end(const char *p) {
    if (!rest_blank(p)) {
        return reject("多了参数");
    }
    return 0;
}

static int cmd_init(char *p) {
    if (need_end(p) != 0) {
        return -1;
    }
    topo_init(&net);
    return 0;
}

static int cmd_node(char *p) {
    char name[TOPO_NAME];
    char flag[16];
    char err[80];
    int stub = 0;
    int rc = take_token(&p, name, (int)sizeof name);

    if (rc != 1) {
        return reject(rc < 0 ? "参数太长" : "缺少结点名");
    }
    rc = take_token(&p, flag, (int)sizeof flag);
    if (rc == 1) {
        if (strcmp(flag, "net") != 0) {
            return reject("结点后面只能写 net");
        }
        stub = 1;
    } else if (rc < 0) {
        return reject("参数太长");
    }
    if (need_end(p) != 0) {
        return -1;
    }
    if (topo_add_node(&net, name, stub, err, (int)sizeof err) < 0) {
        return reject(err);
    }
    return 0;
}

static int cmd_link(char *p) {
    char a[TOPO_NAME];
    char b[TOPO_NAME];
    char err[80];
    int w = 0;

    if (take_token(&p, a, (int)sizeof a) != 1 || take_token(&p, b, (int)sizeof b) != 1) {
        return reject("链路要写两个结点");
    }
    if (take_int(&p, &w) != 1) {
        return reject("链路要写正整数代价");
    }
    if (need_end(p) != 0) {
        return -1;
    }
    if (topo_link(&net, a, b, w, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    return 0;
}

static void print_row(const LsRow *row) {
    if (row->cost < 0) {
        printf("%s - - 不可达\n", net.name[row->dest]);
        return;
    }
    if (row->direct_stub) {
        printf("%s 直连 %d %s\n", net.name[row->dest], row->cost, row->path);
        return;
    }
    printf("%s %s %d %s\n", net.name[row->dest], net.name[row->next], row->cost, row->path);
}

static int cmd_from(char *p) {
    char name[TOPO_NAME];
    char flag[16];
    char err[80];
    LsRow rows[G_NMAX];
    int only = 0;
    int n = 0;
    int src;
    int rc;

    if (take_token(&p, name, (int)sizeof name) != 1) {
        return reject("缺少源结点");
    }
    rc = take_token(&p, flag, (int)sizeof flag);
    if (rc == 1) {
        if (strcmp(flag, "net") != 0) {
            return reject("from 后面只能再写 net");
        }
        only = 1;
    } else if (rc < 0) {
        return reject("参数太长");
    }
    if (need_end(p) != 0) {
        return -1;
    }
    src = topo_find(&net, name);
    if (src < 0) {
        return reject("没有这个源结点");
    }
    if (ls_rows(&net, src, only, rows, G_NMAX, &n, err, (int)sizeof err) != 0) {
        return reject(err);
    }
    printf("【链路状态】源 %s\n", name);
    printf("目的 下一跳 代价 路径\n");
    for (int i = 0; i < n; i++) {
        print_row(&rows[i]);
    }
    return 0;
}

static int cmd_default(char *p) {
    int metric = 0;

    if (take_int(&p, &metric) != 1 || metric <= 0) {
        return reject("Metric 要是正整数");
    }
    if (need_end(p) != 0) {
        return -1;
    }
    printf("LSI 增加 Prefix 0.0.0.0/0 Metric %d\n", metric);
    return 0;
}

int run_ls(char *line) {
    char cmd[16];
    char *p = line;
    int rc = take_token(&p, cmd, (int)sizeof cmd);

    if (rc != 1) {
        return reject(rc < 0 ? "命令太长" : "空命令");
    }
    if (strcmp(cmd, "init") == 0) {
        return cmd_init(p);
    }
    if (strcmp(cmd, "node") == 0) {
        return cmd_node(p);
    }
    if (strcmp(cmd, "link") == 0) {
        return cmd_link(p);
    }
    if (strcmp(cmd, "from") == 0) {
        return cmd_from(p);
    }
    if (strcmp(cmd, "default") == 0) {
        return cmd_default(p);
    }
    return reject("没有这条命令");
}
