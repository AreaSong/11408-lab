#include "bgp.h"
#include "bgp_int.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int from;
    int to;
    char prefix[BGP_PREFIX_MAX];
    int path[BGP_PATH_MAX];
    int npath;
} BgpMsg;

void bgp_init(BgpNet *net) {
    memset(net, 0, sizeof *net);
}

int bgp_parse_int(const char *text, int min, int max, int *out, const char *what, char *err,
                  int err_cap) {
    char *end = NULL;
    unsigned long v;

    if (text[0] < '0' || text[0] > '9') {
        snprintf(err, (size_t)err_cap, "%s要是十进制整数", what);
        return 0;
    }
    errno = 0;
    v = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || v > (unsigned long)max ||
        v < (unsigned long)min) {
        snprintf(err, (size_t)err_cap, "%s要在 %d～%d", what, min, max);
        return 0;
    }
    *out = (int)v;
    return 1;
}

static unsigned int prefix_mask(unsigned int len) {
    if (len == 0) {
        return 0;
    }
    if (len >= 32) {
        return 0xffffffffu;
    }
    return 0xffffffffu << (32 - len);
}

int bgp_prefix_ok(const char *text, char *out, int out_cap, char *err, int err_cap) {
    unsigned int a, b, c, d, len;
    char tail;
    unsigned int ip;
    unsigned int mask;

    if (sscanf(text, "%u.%u.%u.%u/%u%c", &a, &b, &c, &d, &len, &tail) != 5) {
        snprintf(err, (size_t)err_cap, "前缀要写成 a.b.c.d/长度");
        return 0;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255 || len > 32) {
        snprintf(err, (size_t)err_cap, "前缀的地址或长度不合法");
        return 0;
    }
    ip = (a << 24) | (b << 16) | (c << 8) | d;
    mask = prefix_mask(len);
    if ((ip & ~mask) != 0) {
        snprintf(err, (size_t)err_cap, "前缀的主机位必须是 0");
        return 0;
    }
    snprintf(out, (size_t)out_cap, "%u.%u.%u.%u/%u", a, b, c, d, len);
    return 1;
}

int bgp_name_ok(const char *text) {
    int i;
    int n;

    if (text == NULL) {
        return 0;
    }
    n = (int)strlen(text);
    if (n < 1 || n >= BGP_NAME_MAX) {
        return 0;
    }
    if (!isalpha((unsigned char)text[0])) {
        return 0;
    }
    for (i = 1; i < n; i++) {
        if (!isalnum((unsigned char)text[i])) {
            return 0;
        }
    }
    return 1;
}

int bgp_has_as(const BgpNet *net, int number) {
    int i;

    for (i = 0; i < net->nas; i++) {
        if (net->as[i].number == number) {
            return 1;
        }
    }
    return 0;
}

int bgp_add_as(BgpNet *net, int number, char *err, int err_cap) {
    if (number < 1 || number > 65535) {
        snprintf(err, (size_t)err_cap, "AS 编号要在 1～65535");
        return 0;
    }
    if (bgp_has_as(net, number)) {
        snprintf(err, (size_t)err_cap, "AS %d 已经有了", number);
        return 0;
    }
    if (net->nas >= BGP_AS_MAX) {
        snprintf(err, (size_t)err_cap, "自治系统最多 %d 个", BGP_AS_MAX);
        return 0;
    }
    net->as[net->nas++].number = number;
    return 1;
}

static int peer_exists(const BgpNet *net, int a, int b) {
    int i;

    for (i = 0; i < net->npeer; i++) {
        int x = net->peer[i].a;
        int y = net->peer[i].b;
        if ((x == a && y == b) || (x == b && y == a)) {
            return 1;
        }
    }
    return 0;
}

int bgp_add_peer(BgpNet *net, int a, int b, char *err, int err_cap) {
    if (!bgp_has_as(net, a) || !bgp_has_as(net, b)) {
        snprintf(err, (size_t)err_cap, "先用 as 加入这两个自治系统");
        return 0;
    }
    if (a == b) {
        snprintf(err, (size_t)err_cap, "eBGP 邻接要连两个不同的 AS");
        return 0;
    }
    if (peer_exists(net, a, b)) {
        snprintf(err, (size_t)err_cap, "AS %d 和 AS %d 已经相邻", a, b);
        return 0;
    }
    if (net->npeer >= BGP_PEER_MAX) {
        snprintf(err, (size_t)err_cap, "邻接太多");
        return 0;
    }
    net->peer[net->npeer].a = a;
    net->peer[net->npeer].b = b;
    net->npeer++;
    return 1;
}

static int route_index(const BgpNet *net, int holder, const char *prefix) {
    int i;

    for (i = 0; i < net->nroute; i++) {
        if (net->route[i].holder == holder && strcmp(net->route[i].prefix, prefix) == 0) {
            return i;
        }
    }
    return -1;
}

int bgp_add_origin(BgpNet *net, int as, const char *prefix, char *err, int err_cap) {
    BgpRoute *route;

    if (!bgp_has_as(net, as)) {
        snprintf(err, (size_t)err_cap, "还没有 AS %d", as);
        return 0;
    }
    if (route_index(net, as, prefix) >= 0) {
        snprintf(err, (size_t)err_cap, "AS %d 已经有 %s", as, prefix);
        return 0;
    }
    if (net->nroute >= BGP_ROUTE_MAX) {
        snprintf(err, (size_t)err_cap, "路由表满了");
        return 0;
    }
    route = &net->route[net->nroute++];
    memset(route, 0, sizeof *route);
    route->holder = as;
    route->local = 1;
    snprintf(route->prefix, sizeof route->prefix, "%s", prefix);
    return 1;
}

int bgp_path(const BgpNet *net, int holder, const char *prefix, int *path, int cap, int *local) {
    int idx = route_index(net, holder, prefix);
    int i;
    int n;

    if (idx < 0) {
        return -1;
    }
    n = net->route[idx].npath;
    if (local != NULL) {
        *local = net->route[idx].local;
    }
    for (i = 0; i < n && i < cap; i++) {
        path[i] = net->route[idx].path[i];
    }
    return n;
}


static int fill_neighbors(const BgpNet *net, int as, int *out, int cap) {
    int i;
    int n = 0;

    for (i = 0; i < net->npeer; i++) {
        int other = 0;
        if (net->peer[i].a == as) {
            other = net->peer[i].b;
        } else if (net->peer[i].b == as) {
            other = net->peer[i].a;
        } else {
            continue;
        }
        if (n >= cap) {
            return -1;
        }
        out[n++] = other;
    }
    return n;
}

static int with_self(int self, const BgpRoute *route, int *path) {
    int i;

    if (route->npath + 1 > BGP_PATH_MAX) {
        return -1;
    }
    path[0] = self;
    for (i = 0; i < route->npath; i++) {
        path[i + 1] = route->path[i];
    }
    return route->npath + 1;
}

static int path_has(const int *path, int n, int as) {
    int i;

    for (i = 0; i < n; i++) {
        if (path[i] == as) {
            return 1;
        }
    }
    return 0;
}

static int path_same(const int *a, int na, const int *b, int nb) {
    int i;

    if (na != nb) {
        return 0;
    }
    for (i = 0; i < na; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int collect(const BgpNet *net, BgpMsg *msgs, int cap) {
    int r;
    int nmsg = 0;

    for (r = 0; r < net->nroute; r++) {
        int neigh[BGP_PEER_MAX];
        int nn = fill_neighbors(net, net->route[r].holder, neigh, BGP_PEER_MAX);
        int k;

        if (nn < 0) {
            return -1;
        }
        /* 也发给学来的那个邻居。防环靠 AS 路径，不靠水平分割，这样才会看见丢弃。 */
        for (k = 0; k < nn; k++) {
            BgpMsg *m;
            int npath;

            if (nmsg >= cap) {
                return -1;
            }
            m = &msgs[nmsg];
            npath = with_self(net->route[r].holder, &net->route[r], m->path);
            if (npath < 0) {
                return -2;
            }
            m->from = net->route[r].holder;
            m->to = neigh[k];
            m->npath = npath;
            snprintf(m->prefix, sizeof m->prefix, "%s", net->route[r].prefix);
            nmsg++;
        }
    }
    return nmsg;
}

static int install_route(BgpNet *net, const BgpMsg *msg) {
    int idx = route_index(net, msg->to, msg->prefix);
    BgpRoute *route;

    if (idx < 0) {
        if (net->nroute >= BGP_ROUTE_MAX) {
            return -1;
        }
        idx = net->nroute++;
        memset(&net->route[idx], 0, sizeof net->route[idx]);
        net->route[idx].holder = msg->to;
        snprintf(net->route[idx].prefix, sizeof net->route[idx].prefix, "%s", msg->prefix);
    }
    route = &net->route[idx];
    memcpy(route->path, msg->path, (size_t)msg->npath * sizeof(int));
    route->npath = msg->npath;
    route->local = 0;
    return 0;
}

static void future_text(const BgpMsg *msg, char *out, int cap) {
    int announced[BGP_PATH_MAX + 1];
    int i;

    announced[0] = msg->to;
    for (i = 0; i < msg->npath; i++) {
        announced[i + 1] = msg->path[i];
    }
    path_text(announced, msg->npath + 1, out, cap);
}

static void keep_reason(const BgpNet *net, int idx, const BgpMsg *msg, char *why, int cap) {
    char oldp[80];

    path_text(net->route[idx].path, net->route[idx].npath, oldp, (int)sizeof oldp);
    if (net->route[idx].npath == msg->npath) {
        snprintf(why, (size_t)cap, "两条路径都是 %d 个 AS，保留先收到的。", msg->npath);
        return;
    }
    snprintf(why, (size_t)cap, "AS %d 已有路径 %s，长度 %d。新路径长度 %d，更长，保留短的。", msg->to,
             oldp, net->route[idx].npath, msg->npath);
}

static int judge(BgpNet *net, const BgpMsg *msg, char *why, int cap) {
    int idx;
    int old_n;
    char shown[80];
    char next[80];

    path_text(msg->path, msg->npath, shown, (int)sizeof shown);
    if (path_has(msg->path, msg->npath, msg->to)) {
        snprintf(why, (size_t)cap, "AS %d 的编号在路径 %s 里，丢弃。环路在自治系统这一层被挡住。", msg->to,
                 shown);
        return 0;
    }
    idx = route_index(net, msg->to, msg->prefix);
    if (idx >= 0 && net->route[idx].local) {
        snprintf(why, (size_t)cap, "AS %d 自己始发 %s，不改成学来的路径。", msg->to, msg->prefix);
        return 0;
    }
    if (idx >= 0 && path_same(net->route[idx].path, net->route[idx].npath, msg->path, msg->npath)) {
        snprintf(why, (size_t)cap, "AS %d 已有路径 %s，不改。", msg->to, shown);
        return 0;
    }
    if (idx >= 0 && net->route[idx].npath <= msg->npath) {
        keep_reason(net, idx, msg, why, cap);
        return 0;
    }
    old_n = idx >= 0 ? net->route[idx].npath : -1;
    if (install_route(net, msg) != 0) {
        snprintf(why, (size_t)cap, "路由表满了。");
        return 0;
    }
    future_text(msg, next, (int)sizeof next);
    if (old_n >= 0) {
        snprintf(why, (size_t)cap, "新路径长度 %d，比已有的 %d 短，换成新的。再通告时写成 %s。", msg->npath,
                 old_n, next);
        return 1;
    }
    snprintf(why, (size_t)cap, "AS %d 的编号不在路径里，收下。再通告时写成 %s。", msg->to, next);
    return 1;
}

int bgp_step(BgpNet *net, FILE *fp) {
    /* 先按本轮开始时的表生成通告。本轮新收下的，要等下一轮才继续传。 */
    static BgpMsg msgs[BGP_ROUTE_MAX * BGP_AS_MAX];
    int nmsg;
    int i;
    int got = 0;
    char why[180];
    char shown[80];

    net->round++;
    say(fp, "【第 %d 轮】\n", net->round);
    nmsg = collect(net, msgs, (int)(sizeof msgs / sizeof msgs[0]));
    if (nmsg == -2) {
        say(fp, "AS 路径超过 %d 个，这一轮停下来。\n结果：本轮收下 0 条\n", BGP_PATH_MAX);
        return 0;
    }
    if (nmsg < 0) {
        say(fp, "通告太多，这一轮停下来。\n结果：本轮收下 0 条\n");
        return 0;
    }
    if (nmsg == 0) {
        say(fp, "没有路由可通告。\n结果：本轮收下 0 条\n");
        return 0;
    }
    for (i = 0; i < nmsg; i++) {
        path_text(msgs[i].path, msgs[i].npath, shown, (int)sizeof shown);
        say(fp, "AS %d → AS %d：通告 %s，AS 路径 %s。\n", msgs[i].from, msgs[i].to, msgs[i].prefix,
            shown);
        if (judge(net, &msgs[i], why, (int)sizeof why)) {
            got++;
        }
        say(fp, "%s\n", why);
    }
    say(fp, "结果：本轮收下 %d 条\n", got);
    return got;
}

