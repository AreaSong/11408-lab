#include "route.h"

#include "qty.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int link_ns(const RouteLink *links, int nlinks, const char *a, const char *b,
                   long long *ns) {
    int i;

    for (i = 0; i < nlinks; i++) {
        int ab = strcmp(links[i].a, a) == 0 && strcmp(links[i].b, b) == 0;
        int ba = strcmp(links[i].a, b) == 0 && strcmp(links[i].b, a) == 0;
        if (ab || ba) {
            *ns = links[i].ns;
            return 1;
        }
    }
    return 0;
}

bool route_path_ns(const RouteLink *links, int nlinks, const RoutePath *path, long long *ns) {
    long long sum = 0;
    int i;

    if (links == NULL || path == NULL || ns == NULL || path->nodes < 2) {
        return false;
    }
    for (i = 0; i < path->nodes - 1; i++) {
        long long hop = 0;
        if (!link_ns(links, nlinks, path->name[i], path->name[i + 1], &hop)) {
            return false;
        }
        if (sum > LLONG_MAX - hop) {
            return false;
        }
        sum += hop;
    }
    *ns = sum;
    return true;
}

bool route_arrivals(long long setup, long long interval, const long long *path_ns, int n,
                    long long *arrivals) {
    int i;

    if (setup < 0 || interval < 0 || path_ns == NULL || arrivals == NULL || n < 0) {
        return false;
    }
    for (i = 0; i < n; i++) {
        long long send;
        if (interval > 0 && i > LLONG_MAX / interval) {
            return false;
        }
        send = (long long)i * interval;
        if (setup > LLONG_MAX - send || path_ns[i] < 0) {
            return false;
        }
        if (setup + send > LLONG_MAX - path_ns[i]) {
            return false;
        }
        arrivals[i] = setup + send + path_ns[i];
    }
    return true;
}

void route_order(const long long *arrivals, int n, int *order) {
    int i;

    for (i = 0; i < n; i++) {
        order[i] = i;
    }
    for (i = 1; i < n; i++) {
        int key = order[i];
        int j = i;
        while (j > 0 && arrivals[order[j - 1]] > arrivals[key]) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = key;
    }
}

static void fmt_nodes(char *buf, size_t n, const RoutePath *path) {
    size_t used = 0;
    int i;

    buf[0] = '\0';
    for (i = 0; i < path->nodes; i++) {
        int wrote = snprintf(buf + used, n - used, "%s%s", i ? "-" : "", path->name[i]);
        if (wrote < 0 || (size_t)wrote >= n - used) {
            return;
        }
        used += (size_t)wrote;
    }
}

static int same_send_order(const int *order, int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (order[i] != i) {
            return 0;
        }
    }
    return 1;
}

static int has_tie(const long long *arrivals, const int *order, int n) {
    int i;

    for (i = 1; i < n; i++) {
        if (arrivals[order[i]] == arrivals[order[i - 1]]) {
            return 1;
        }
    }
    return 0;
}

static void print_order(FILE *out, const RouteScene *scene, const int *order) {
    int i;

    fputs("到达顺序：", out);
    for (i = 0; i < scene->npackets; i++) {
        fprintf(out, "%s%d", i ? " " : "", scene->packets[order[i]].id);
    }
    fputs("\n", out);
}

static int print_mode(FILE *out, const char *title, const RouteScene *scene,
                      const RoutePath *paths, const long long *path_ns, long long setup,
                      const char *how, int *ids_out) {
    long long arrivals[ROUTE_MAX_PKTS];
    int order[ROUTE_MAX_PKTS];
    int i;

    fprintf(out, "【%s】\n", title);
    if (!route_arrivals(setup, scene->interval_ns, path_ns, scene->npackets, arrivals)) {
        fputs("到达时刻溢出\n", stderr);
        return 1;
    }
    route_order(arrivals, scene->npackets, order);
    for (i = 0; i < scene->npackets; i++) {
        char nodes[128], delay[64], send[64], arrive[64];
        long long send_ns = (long long)i * scene->interval_ns;

        fmt_nodes(nodes, sizeof nodes, &paths[i]);
        fmt_time_one(delay, sizeof delay, path_ns[i]);
        fmt_time_one(send, sizeof send, send_ns);
        fmt_time_one(arrive, sizeof arrive, arrivals[i]);
        fprintf(out, "分组 %d 路径 %s 时延 %s，发送 %s，到达 %s（%s）\n",
                scene->packets[i].id, nodes, delay, send, arrive, how);
    }
    print_order(out, scene, order);
    if (has_tie(arrivals, order, scene->npackets)) {
        fputs("有分组同时到达，顺序里把先发送的排在前面。\n", out);
    }
    if (same_send_order(order, scene->npackets)) {
        fputs("和发送顺序相同。\n", out);
    } else {
        fputs("和发送顺序不同。\n", out);
    }
    for (i = 0; i < scene->npackets; i++) {
        ids_out[i] = scene->packets[order[i]].id;
    }
    return 0;
}

static int resolve(const RouteScene *scene, long long *dg, RoutePath *vc_paths) {
    long long ignore = 0;
    int i;

    if (!route_path_ns(scene->links, scene->nlinks, &scene->vc, &ignore)) {
        fprintf(stderr, "虚电路路径上有未给出的链路\n");
        return 0;
    }
    for (i = 0; i < scene->npackets; i++) {
        vc_paths[i] = scene->vc;
        if (!route_path_ns(scene->links, scene->nlinks, &scene->packets[i].path, &dg[i])) {
            fprintf(stderr, "分组 %d 的路径上有未给出的链路\n", scene->packets[i].id);
            return 0;
        }
    }
    return 1;
}

static void print_links(FILE *out, const RouteScene *scene) {
    int i;

    for (i = 0; i < scene->nlinks; i++) {
        char delay[64];
        fmt_time_one(delay, sizeof delay, scene->links[i].ns);
        fprintf(out, "链路 %s-%s = %s\n", scene->links[i].a, scene->links[i].b, delay);
    }
}

static void print_banner(FILE *out, const RouteScene *scene, long long vc_delay) {
    char interval[64], setup[64], nodes[128], delay[64];

    fmt_time_one(interval, sizeof interval, scene->interval_ns);
    fmt_time_one(setup, sizeof setup, scene->setup_ns);
    fmt_nodes(nodes, sizeof nodes, &scene->vc);
    fmt_time_one(delay, sizeof delay, vc_delay);
    fputs("【数据报与虚电路】\n", out);
    fprintf(out, "发送间隔 = %s\n", interval);
    fprintf(out, "虚电路建立时延 = %s。建立在发送数据之前，只做一次。\n", setup);
    fputs("数据报不建立连接，每个分组按自己的路径走。\n", out);
    fputs("虚电路建立时选定路径，之后全部分组走同一条路。\n", out);
    fputs("虚电路不预分配带宽。预分配带宽、独占通路的是电路交换。\n", out);
    print_links(out, scene);
    fprintf(out, "虚电路路径 %s 时延 %s\n", nodes, delay);
}

static void print_ids(FILE *out, const char *name, const int *ids, int n) {
    int i;

    fprintf(out, "%s", name);
    for (i = 0; i < n; i++) {
        fprintf(out, "%s%d", i ? " " : "", ids[i]);
    }
}

int route_report(FILE *out, const RouteScene *scene) {
    long long dg_ns[ROUTE_MAX_PKTS];
    long long vc_ns[ROUTE_MAX_PKTS];
    RoutePath dg_paths[ROUTE_MAX_PKTS];
    RoutePath vc_paths[ROUTE_MAX_PKTS];
    int dg_ids[ROUTE_MAX_PKTS];
    int vc_ids[ROUTE_MAX_PKTS];
    long long vc_delay = 0;
    int i;

    if (scene->nlinks < 1 || !scene->has_vc || scene->npackets < 1) {
        fprintf(stderr, "至少要有一条链路、一条虚电路和一个分组\n");
        return 1;
    }
    for (i = 0; i < scene->npackets; i++) {
        dg_paths[i] = scene->packets[i].path;
    }
    if (!resolve(scene, dg_ns, vc_paths)) {
        return 1;
    }
    if (!route_path_ns(scene->links, scene->nlinks, &scene->vc, &vc_delay)) {
        return 1;
    }
    for (i = 0; i < scene->npackets; i++) {
        vc_ns[i] = vc_delay;
    }
    print_banner(out, scene, vc_delay);
    if (print_mode(out, "数据报", scene, dg_paths, dg_ns, 0, "发送 + 时延", dg_ids) != 0) {
        return 1;
    }
    if (print_mode(out, "虚电路", scene, vc_paths, vc_ns, scene->setup_ns,
                   "建立 + 发送 + 时延", vc_ids) != 0) {
        return 1;
    }
    fputs("结果：数据报 ", out);
    print_ids(out, "", dg_ids, scene->npackets);
    fputs("，虚电路 ", out);
    print_ids(out, "", vc_ids, scene->npackets);
    fputs("\n", out);
    return 0;
}
