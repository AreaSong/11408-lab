#include "run_fwd.h"

#include "addr.h"
#include "line.h"
#include "pkt.h"
#include "trie.h"

#include <stdio.h>
#include <string.h>

static Fib fib;

void fwd_init(void) {
    fib_init(&fib);
}

static int iface_ok(const char *text) {
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
        if (n >= IFACE_MAX) {
            return 0;
        }
    }
    return 1;
}

static void hop_text(const Route *route, char *out, int cap) {
    if (route->direct) {
        snprintf(out, (size_t)cap, "直连");
        return;
    }
    ip_fmt(route->nexthop, out, cap);
}

static const Route *find_route(uint32_t net, int prefix) {
    int i;

    for (i = 0; i < fib.n; i++) {
        if (fib.item[i].network == net && fib.item[i].prefix == prefix) {
            return &fib.item[i];
        }
    }
    return &fib.item[fib.n - 1];
}

static int report_add(uint32_t net, int prefix, int rc) {
    const Route *route = find_route(net, prefix);
    char text[32];
    char hop[32];

    prefix_fmt(route->network, route->prefix, text, (int)sizeof text);
    hop_text(route, hop, (int)sizeof hop);
    printf("【路由表】\n");
    if (route->prefix == 0) {
        printf("%s，下一跳 %s，接口 %s。默认路由挂在 Trie 的根上。\n", text, hop, route->iface);
    } else {
        printf("%s，下一跳 %s，接口 %s。这条前缀写在走完高 %d 位之后的结点上。\n", text, hop, route->iface,
               route->prefix);
    }
    if (rc == 2) {
        printf("同一条前缀已经有了，下一跳和接口按这次输入替换。\n");
    }
    printf("结果：已插入 %s\n", text);
    return 0;
}

static int add_route(uint32_t net, int prefix, int direct, uint32_t hop, const char *iface) {
    char err[80];
    int rc = fib_add(&fib, net, prefix, direct, hop, iface, err, (int)sizeof err);

    if (rc == 0) {
        return reject(err);
    }
    return report_add(net, prefix, rc);
}

static int cmd_default(char *line) {
    char via[16];
    char ip[32];
    char dev[8];
    char iface[IFACE_MAX];
    uint32_t hop;

    if (take_token(&line, via, (int)sizeof via) != 1 || strcmp(via, "via") != 0) {
        return reject("用法：default via <下一跳> dev <接口>");
    }
    if (take_token(&line, ip, (int)sizeof ip) != 1 || !ip_parse(ip, &hop) || hop == 0) {
        return reject("下一跳不合法");
    }
    if (take_token(&line, dev, (int)sizeof dev) != 1 || strcmp(dev, "dev") != 0) {
        return reject("用法：default via <下一跳> dev <接口>");
    }
    if (take_token(&line, iface, (int)sizeof iface) != 1 || !iface_ok(iface) || !rest_blank(line)) {
        return reject("接口只能是字母和数字");
    }
    return add_route(0, 0, 0, hop, iface);
}

static int cmd_route(char *line) {
    char pref[32];
    char word[16];
    char hop_text_in[32];
    char iface[IFACE_MAX];
    uint32_t net;
    uint32_t hop = 0;
    int prefix;
    int direct = 1;

    if (take_token(&line, pref, (int)sizeof pref) != 1 || !prefix_parse(pref, &net, &prefix)) {
        return reject("用法：route <网络>/<前缀> [via <下一跳>] dev <接口>");
    }
    if (take_token(&line, word, (int)sizeof word) != 1) {
        return reject("用法：route <网络>/<前缀> [via <下一跳>] dev <接口>");
    }
    if (strcmp(word, "via") == 0) {
        direct = 0;
        if (take_token(&line, hop_text_in, (int)sizeof hop_text_in) != 1 || !ip_parse(hop_text_in, &hop)) {
            return reject("下一跳不合法");
        }
        if (take_token(&line, word, (int)sizeof word) != 1) {
            return reject("缺少 dev");
        }
    }
    if (strcmp(word, "dev") != 0) {
        return reject("用法：route <网络>/<前缀> [via <下一跳>] dev <接口>");
    }
    if (take_token(&line, iface, (int)sizeof iface) != 1 || !iface_ok(iface) || !rest_blank(line)) {
        return reject("接口只能是字母和数字");
    }
    return add_route(net, prefix, direct, hop, iface);
}

static void print_prefix_byte(uint32_t ip, int prefix) {
    int index;
    int bits;
    unsigned byte;
    char text[9];

    if (prefix <= 0 || prefix >= 32 || prefix % 8 == 0) {
        return;
    }
    index = prefix / 8;
    bits = prefix % 8;
    byte = (ip >> (8 * (3 - index))) & 255u;
    bits8(byte, text);
    printf("第 %d 字节 %u = %s，这条前缀用到其中高 %d 位\n", index + 1, byte, text, bits);
}

static int cmd_lookup(char *line) {
    char tok[32];
    char shown[32];
    char hop[32];
    uint32_t ip;
    int idx[FIB_MAX];
    int n;
    int i;
    int best;

    if (take_token(&line, tok, (int)sizeof tok) != 1 || !rest_blank(line)) {
        return reject("用法：lookup <目的 IP>");
    }
    if (!ip_parse(tok, &ip)) {
        return reject("目的 IP 不合法");
    }
    ip_fmt(ip, shown, (int)sizeof shown);
    printf("【最长前缀匹配】\n");
    printf("目的 %s。从高位沿 Trie 往下走，记下沿途每一个写了前缀的结点。\n", shown);
    n = fib_matches(&fib, ip, idx, FIB_MAX);
    if (n == 0) {
        printf("没有一条前缀匹配。分组丢弃，向源主机发 ICMP 终点不可达。\n");
        printf("结果：没有匹配的路由，ICMP 终点不可达，类型 3，代码 0\n");
        return 0;
    }
    for (i = 0; i < n; i++) {
        const Route *route = &fib.item[idx[i]];

        prefix_fmt(route->network, route->prefix, shown, (int)sizeof shown);
        hop_text(route, hop, (int)sizeof hop);
        printf("%s 匹配，前缀 %d 位，下一跳 %s，接口 %s\n", shown, route->prefix, hop, route->iface);
        print_prefix_byte(ip, route->prefix);
    }
    best = fib_lookup(&fib, ip);
    if (best < 0 || fib.item[best].prefix != fib.item[idx[n - 1]].prefix) {
        return reject("Trie 和逐条比较的结果不一致");
    }
    hop_text(&fib.item[best], hop, (int)sizeof hop);
    if (n == 1 && fib.item[best].prefix == 0) {
        printf("只有默认路由匹配。\n");
    } else {
        printf("候选里最长的是 %d 位。\n", fib.item[best].prefix);
    }
    printf("结果：下一跳 %s，接口 %s\n", hop, fib.item[best].iface);
    return 0;
}

static int parse_u(const char *text, unsigned *out, unsigned lo, unsigned hi) {
    unsigned value;
    int n = 0;

    if (sscanf(text, "%u%n", &value, &n) != 1 || text[n] != '\0' || value < lo || value > hi) {
        return 0;
    }
    *out = value;
    return 1;
}

static int cmd_ttl(char *line) {
    char a[16];
    char b[16];
    char c[16];
    unsigned ttl;
    unsigned hops;

    if (take_token(&line, a, (int)sizeof a) != 1 || take_token(&line, b, (int)sizeof b) != 1 ||
        take_token(&line, c, (int)sizeof c) != 1 || !rest_blank(line) || strcmp(b, "hops") != 0) {
        return reject("用法：ttl <发出时> hops <路由器台数>");
    }
    if (!parse_u(a, &ttl, 1, 255) || !parse_u(c, &hops, 0, 255)) {
        return reject("TTL 要在 1 到 255，路由器台数要在 0 到 255");
    }
    printf("【TTL】\n");
    printf("发出时 TTL = %u。TTL 是跳数限制，每过一台路由器减 1。\n", ttl);
    printf("路径上有 %u 台路由器。\n", hops);
    if (hops >= ttl) {
        printf("%u - %u 不是正数，分组在中途被丢掉。\n", ttl, hops);
        printf("结果：丢弃，ICMP 时间超过，类型 11，代码 0\n");
        return 0;
    }
    printf("到达时 TTL = %u - %u = %u\n", ttl, hops, ttl - hops);
    printf("结果：TTL %u\n", ttl - hops);
    return 0;
}

static int parse_byte(const char *text, unsigned char *out) {
    unsigned value;
    int n = 0;

    if (sscanf(text, "%x%n", &value, &n) != 1 || text[n] != '\0' || strlen(text) != 2 || value > 255u) {
        return 0;
    }
    *out = (unsigned char)value;
    return 1;
}

static int read_bytes(char **line, unsigned char *buf, int cap, int *n, int have_first, unsigned char first) {
    char tok[8];
    int rc;

    *n = 0;
    if (have_first) {
        buf[(*n)++] = first;
    }
    for (;;) {
        rc = take_token(line, tok, (int)sizeof tok);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            return reject("这一项太长");
        }
        if (*n >= cap || !parse_byte(tok, &buf[*n])) {
            return reject("首部字节要写成两位十六进制");
        }
        (*n)++;
    }
    return 0;
}

static void print_hex(const unsigned char *bytes, int n) {
    int i;

    for (i = 0; i < n; i++) {
        printf("%s%02x", i ? " " : "", bytes[i]);
    }
    printf("\n");
}

static void print_fwd(const FwdResult *fwd, int best) {
    char old_src[16];
    char new_src[16];
    char dst[16];
    char hop[32];

    ip_fmt(fwd->old_src, old_src, (int)sizeof old_src);
    ip_fmt(fwd->new_src, new_src, (int)sizeof new_src);
    ip_fmt(fwd->dst, dst, (int)sizeof dst);
    if (fwd->nat) {
        printf("NAT：源 %s 改成 %s。\n", old_src, new_src);
    }
    printf("TTL：%u - 1 = %u\n", fwd->old_ttl, fwd->new_ttl);
    printf("目的地址 %s 不变。\n", dst);
    if (fwd->total > 1500u) {
        printf("总长度 %u 字节大于以太网 MTU 1500 字节。分片还会改总长度、标志和片偏移，这里不切。\n",
               fwd->total);
    } else {
        printf("总长度 %u 字节不超过以太网 MTU 1500 字节，总长度、标志、片偏移不变。\n", fwd->total);
    }
    printf("首部校验和按 16 位反码求和重算：0x%04x → 0x%04x\n", fwd->old_sum, fwd->new_sum);
    printf("转发后的首部：");
    print_hex(fwd->raw, fwd->hdr_len);
    if (best >= 0) {
        hop_text(&fib.item[best], hop, (int)sizeof hop);
        printf("结果：下一跳 %s，接口 %s；%s\n", hop, fib.item[best].iface,
               fwd->nat ? "改变了源地址、TTL、首部校验和" : "改变了 TTL、首部校验和");
        return;
    }
    printf("结果：%s\n", fwd->nat ? "改变了源地址、TTL、首部校验和" : "改变了 TTL、首部校验和");
}

static int route_of(uint32_t dst) {
    char ip[16];
    char net[32];
    char hop[32];
    int best;

    if (fib.n == 0) {
        return -1;
    }
    best = fib_lookup(&fib, dst);
    ip_fmt(dst, ip, (int)sizeof ip);
    if (best < 0) {
        printf("目的 %s 没有匹配的路由。\n", ip);
        printf("结果：丢弃，ICMP 终点不可达，类型 3，代码 0\n");
        return -2;
    }
    prefix_fmt(fib.item[best].network, fib.item[best].prefix, net, (int)sizeof net);
    hop_text(&fib.item[best], hop, (int)sizeof hop);
    printf("查表：目的 %s 匹配 %s，下一跳 %s，接口 %s\n", ip, net, hop, fib.item[best].iface);
    return best;
}

static void print_time_exceeded(unsigned ttl) {
    printf("TTL：%u - 1 = 0，不再转发。\n", ttl);
    printf("向源主机发 ICMP 时间超过，类型 11，代码 0（传输期间生存时间超时）。\n");
    printf("ICMP 的数据部分带上原 IP 首部，再加原数据的前 8 字节。\n");
    printf("结果：丢弃，ICMP 时间超过，类型 11，代码 0\n");
}

static int cmd_pkt(char *line) {
    char tok[32];
    unsigned char buf[IP_HDR_MAX];
    unsigned char first = 0;
    int have_first = 0;
    int n = 0;
    int do_nat = 0;
    int best = -1;
    uint32_t nat_src = 0;
    IpView view;
    FwdResult fwd;
    char err[80];

    if (take_token(&line, tok, (int)sizeof tok) != 1) {
        return reject("用法：pkt [nat <新源地址>] <两位十六进制> ...");
    }
    if (strcmp(tok, "nat") == 0) {
        do_nat = 1;
        if (take_token(&line, tok, (int)sizeof tok) != 1 || !ip_parse(tok, &nat_src) || nat_src == 0) {
            return reject("nat 后面要写新的源地址");
        }
    } else if (!parse_byte(tok, &first)) {
        return reject("首部字节要写成两位十六进制");
    } else {
        have_first = 1;
    }
    if (read_bytes(&line, buf, IP_HDR_MAX, &n, have_first, first) != 0) {
        return -1;
    }
    if (!ip_load(buf, n, &view, err, (int)sizeof err)) {
        return reject(err);
    }
    printf("【分组转发】\n");
    best = route_of(view.dst);
    if (best == -2) {
        return 0;
    }
    ip_forward(&view, do_nat, nat_src, &fwd);
    if (fwd.dropped) {
        print_time_exceeded(fwd.old_ttl);
        return 0;
    }
    print_fwd(&fwd, best);
    return 0;
}

int run_fwd(char *line) {
    char cmd[16];

    if (take_token(&line, cmd, (int)sizeof cmd) != 1) {
        return reject("缺少命令");
    }
    if (strcmp(cmd, "route") == 0) {
        return cmd_route(line);
    }
    if (strcmp(cmd, "default") == 0) {
        return cmd_default(line);
    }
    if (strcmp(cmd, "lookup") == 0) {
        return cmd_lookup(line);
    }
    if (strcmp(cmd, "ttl") == 0) {
        return cmd_ttl(line);
    }
    if (strcmp(cmd, "pkt") == 0) {
        return cmd_pkt(line);
    }
    if (strcmp(cmd, "reset") == 0) {
        if (!rest_blank(line)) {
            return reject("reset 没有参数");
        }
        fib_clear(&fib);
        printf("【路由表】\nTrie 已清空。\n结果：路由表已清空\n");
        return 0;
    }
    return reject("不认识的命令");
}
