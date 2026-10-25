#include "run_nat.h"

#include "addr.h"
#include "line.h"
#include "nat.h"

#include <stdio.h>
#include <string.h>

static NatBox boxes[NAT_BOXES];

static int name_ok(const char *text) {
    int n = 0;

    if (text[0] == '\0') {
        return 0;
    }
    while (text[n] != '\0') {
        char c = text[n];

        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
            return 0;
        }
        n++;
        if (n >= 8) {
            return 0;
        }
    }
    return 1;
}

static NatBox *find_box(const char *name) {
    int i;
    int free_i = -1;

    for (i = 0; i < NAT_BOXES; i++) {
        if (boxes[i].live && strcmp(boxes[i].name, name) == 0) {
            return &boxes[i];
        }
        if (!boxes[i].live && free_i < 0) {
            free_i = i;
        }
    }
    if (free_i < 0) {
        return NULL;
    }
    natbox_init(&boxes[free_i], name);
    return &boxes[free_i];
}

static int parse_proto(const char *text, int *proto) {
    if (strcmp(text, "tcp") == 0) {
        *proto = NAT_TCP;
        return 1;
    }
    if (strcmp(text, "udp") == 0) {
        *proto = NAT_UDP;
        return 1;
    }
    return 0;
}

static const char *proto_name(int proto) {
    return proto == NAT_UDP ? "UDP" : "TCP";
}

static int parse_port(const char *text, unsigned *port) {
    unsigned value;
    int n = 0;

    if (sscanf(text, "%u%n", &value, &n) != 1 || text[n] != '\0' || value == 0 || value > 65535u) {
        return 0;
    }
    *port = value;
    return 1;
}

static void fmt_end(uint32_t ip, unsigned port, char *out, int cap) {
    char addr[16];

    ip_fmt(ip, addr, (int)sizeof addr);
    snprintf(out, (size_t)cap, "%s:%u", addr, port);
}

static int cmd_map(NatBox *box, char *line) {
    char proto_tok[8];
    char a[32];
    char b[16];
    char c[32];
    char d[16];
    int proto;
    unsigned pp;
    unsigned prip;
    uint32_t pub;
    uint32_t pri;
    char err[80];
    char left[32];
    char right[32];

    if (take_token(&line, proto_tok, (int)sizeof proto_tok) != 1 || !parse_proto(proto_tok, &proto)) {
        return reject("用法：<名字> map <tcp|udp> <外网 IP> <外网端口> <内网 IP> <内网端口>");
    }
    if (take_token(&line, a, (int)sizeof a) != 1 || !ip_parse(a, &pub) || pub == 0) {
        return reject("外网 IP 不合法");
    }
    if (take_token(&line, b, (int)sizeof b) != 1 || !parse_port(b, &pp)) {
        return reject("外网端口要在 1 到 65535");
    }
    if (take_token(&line, c, (int)sizeof c) != 1 || !ip_parse(c, &pri) || pri == 0) {
        return reject("内网 IP 不合法");
    }
    if (take_token(&line, d, (int)sizeof d) != 1 || !parse_port(d, &prip) || !rest_blank(line)) {
        return reject("内网端口要在 1 到 65535");
    }
    if (!natbox_map(box, proto, pub, pp, pri, prip, err, (int)sizeof err)) {
        return reject(err);
    }
    fmt_end(pub, pp, left, (int)sizeof left);
    fmt_end(pri, prip, right, (int)sizeof right);
    printf("【%s NAT 表】\n", box->name);
    printf("静态映射：外网 %s ↔ 内网 %s，协议 %s。\n", left, right, proto_name(proto));
    printf("入站分组用目的地址加目的端口，在散列里查这把公网键。\n");
    printf("结果：外网 %s，内网 %s\n", left, right);
    return 0;
}

static int cmd_basic(NatBox *box, char *line) {
    char tok[32];
    uint32_t pub;
    char err[80];
    char shown[16];

    if (take_token(&line, tok, (int)sizeof tok) != 1 || !ip_parse(tok, &pub) || pub == 0 || !rest_blank(line)) {
        return reject("用法：<名字> basic <公网 IP>");
    }
    if (!natbox_basic(box, pub, err, (int)sizeof err)) {
        return reject(err);
    }
    ip_fmt(pub, shown, (int)sizeof shown);
    printf("【%s 基本 NAT】\n", box->name);
    printf("公网地址 %s。基本 NAT 只替换 IP，端口留着。\n", shown);
    printf("结果：公网地址 %s\n", shown);
    return 0;
}

static int cmd_pool(NatBox *box, char *line) {
    char ip_tok[32];
    char lo_tok[16];
    char hi_tok[16];
    uint32_t pub;
    unsigned lo;
    unsigned hi;
    char err[80];
    char shown[16];

    if (take_token(&line, ip_tok, (int)sizeof ip_tok) != 1 || !ip_parse(ip_tok, &pub) || pub == 0) {
        return reject("用法：<名字> pool <公网 IP> <起始端口> <结束端口>");
    }
    if (take_token(&line, lo_tok, (int)sizeof lo_tok) != 1 || !parse_port(lo_tok, &lo)) {
        return reject("起始端口要在 1 到 65535");
    }
    if (take_token(&line, hi_tok, (int)sizeof hi_tok) != 1 || !parse_port(hi_tok, &hi) || !rest_blank(line)) {
        return reject("结束端口要在 1 到 65535");
    }
    if (!natbox_pool(box, pub, lo, hi, err, (int)sizeof err)) {
        return reject(err);
    }
    ip_fmt(pub, shown, (int)sizeof shown);
    printf("【%s NAPT】\n", box->name);
    printf("公网地址 %s，端口从 %u 分到 %u。多个内网主机共用这一地址，靠端口区分。\n", shown, lo, hi);
    printf("结果：公网地址 %s，端口 %u-%u\n", shown, lo, hi);
    return 0;
}

static int read_flow(char **line, Flow *flow) {
    char proto_tok[8];
    char a[32];
    char b[16];
    char c[32];
    char d[16];

    memset(flow, 0, sizeof *flow);
    if (take_token(line, proto_tok, (int)sizeof proto_tok) != 1 || !parse_proto(proto_tok, &flow->proto)) {
        return reject("协议要写 tcp 或 udp");
    }
    if (take_token(line, a, (int)sizeof a) != 1 || !ip_parse(a, &flow->src) || flow->src == 0) {
        return reject("源 IP 不合法");
    }
    if (take_token(line, b, (int)sizeof b) != 1 || !parse_port(b, &flow->sport)) {
        return reject("源端口要在 1 到 65535");
    }
    if (take_token(line, c, (int)sizeof c) != 1 || !ip_parse(c, &flow->dst) || flow->dst == 0) {
        return reject("目的 IP 不合法");
    }
    if (take_token(line, d, (int)sizeof d) != 1 || !parse_port(d, &flow->dport) || !rest_blank(*line)) {
        return reject("目的端口要在 1 到 65535");
    }
    return 0;
}

static int cmd_emit(const char *name, char *line) {
    Flow flow;
    char src[32];
    char dst[32];

    if (read_flow(&line, &flow) != 0) {
        return -1;
    }
    fmt_end(flow.src, flow.sport, src, (int)sizeof src);
    fmt_end(flow.dst, flow.dport, dst, (int)sizeof dst);
    printf("【%s 发出】\n", name);
    printf("源 %s，目的 %s。这一步还没进路由器。\n", src, dst);
    printf("结果：源 %s，目的 %s\n", src, dst);
    return 0;
}

static int cmd_out(NatBox *box, char *line) {
    Flow before;
    Flow after;
    char err[80];
    char src0[32];
    char src1[32];
    char dst[32];

    if (read_flow(&line, &before) != 0) {
        return -1;
    }
    after = before;
    if (!natbox_out(box, &after, err, (int)sizeof err)) {
        return reject(err);
    }
    fmt_end(before.src, before.sport, src0, (int)sizeof src0);
    fmt_end(after.src, after.sport, src1, (int)sizeof src1);
    fmt_end(after.dst, after.dport, dst, (int)sizeof dst);
    printf("【%s 出站】\n", box->name);
    printf("按（协议，内网地址，内网端口）散列查表。\n");
    if (after.created && box->mode == NAT_BASIC) {
        printf("表里没有这条，新建映射。基本 NAT 不改端口。\n");
    } else if (after.created) {
        printf("表里没有这条，从端口池分一个公网端口。\n");
    } else {
        printf("表里已经有这条，沿用原来的公网地址和端口。\n");
    }
    printf("源 %s → %s\n", src0, src1);
    printf("目的 %s 不变。\n", dst);
    printf("结果：源 %s，目的 %s\n", src1, dst);
    return 0;
}

static int cmd_in(NatBox *box, char *line) {
    Flow before;
    Flow after;
    char err[80];
    char dst0[32];
    char dst1[32];
    char src[32];

    if (read_flow(&line, &before) != 0) {
        return -1;
    }
    after = before;
    if (!natbox_in(box, &after, err, (int)sizeof err)) {
        return reject(err);
    }
    fmt_end(before.dst, before.dport, dst0, (int)sizeof dst0);
    fmt_end(after.dst, after.dport, dst1, (int)sizeof dst1);
    fmt_end(after.src, after.sport, src, (int)sizeof src);
    printf("【%s 入站】\n", box->name);
    printf("按（协议，目的地址，目的端口）散列查公网侧的键。\n");
    printf("目的 %s → %s\n", dst0, dst1);
    printf("源 %s 不变。\n", src);
    printf("结果：源 %s，目的 %s\n", src, dst1);
    return 0;
}

int run_nat(char *line) {
    char name[16];
    char cmd[16];
    NatBox *box;

    if (take_token(&line, name, (int)sizeof name) != 1 || !name_ok(name)) {
        return reject("用法：<名字> <map|basic|pool|emit|out|in> ...");
    }
    if (take_token(&line, cmd, (int)sizeof cmd) != 1) {
        return reject("缺少动作");
    }
    if (strcmp(cmd, "emit") == 0) {
        return cmd_emit(name, line);
    }
    box = find_box(name);
    if (box == NULL) {
        return reject("NAT 路由器最多 4 台");
    }
    if (strcmp(cmd, "map") == 0) {
        return cmd_map(box, line);
    }
    if (strcmp(cmd, "basic") == 0) {
        return cmd_basic(box, line);
    }
    if (strcmp(cmd, "pool") == 0) {
        return cmd_pool(box, line);
    }
    if (strcmp(cmd, "out") == 0) {
        return cmd_out(box, line);
    }
    if (strcmp(cmd, "in") == 0) {
        return cmd_in(box, line);
    }
    return reject("不认识的动作");
}
