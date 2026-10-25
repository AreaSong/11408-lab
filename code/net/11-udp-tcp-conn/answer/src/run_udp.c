#include "run_udp.h"

#include "scan.h"
#include "udp_show.h"

#include <string.h>

static int cmd_sum(char *p) {
    char a[32];
    char b[32];
    unsigned short left;
    unsigned short right;

    if (take_token(&p, a, (int)sizeof a) != 1 || take_token(&p, b, (int)sizeof b) != 1) {
        return reject("sum 需要两个 16 位二进制数");
    }
    if (!parse_bits16(a, &left) || !parse_bits16(b, &right) || !rest_blank(p)) {
        return reject("每个数必须是 16 位 0/1，后面不能再有内容");
    }
    udp_show_sum(left, right);
    return 0;
}

static int cmd_build(char *p) {
    char ipa[32];
    char ipb[32];
    char spa[16];
    char dpa[16];
    char hex[UDP_DATA_MAX * 2 + 1];
    unsigned char src[4];
    unsigned char dst[4];
    unsigned char data[UDP_DATA_MAX];
    unsigned short sp;
    unsigned short dp;
    int n = 0;
    UdpGram g;
    const char *err;
    int got;

    if (take_token(&p, ipa, (int)sizeof ipa) != 1 || take_token(&p, ipb, (int)sizeof ipb) != 1 ||
        take_token(&p, spa, (int)sizeof spa) != 1 || take_token(&p, dpa, (int)sizeof dpa) != 1) {
        return reject("build 需要源 IP、目的 IP、源端口、目的端口和数据");
    }
    got = take_token(&p, hex, (int)sizeof hex);
    if (got < 0) {
        return reject("数据太长");
    }
    if (got != 1 || !rest_blank(p)) {
        return reject("build 的数据用十六进制，没有数据就写 -");
    }
    if (!parse_ip(ipa, src) || !parse_ip(ipb, dst) || !parse_port(spa, &sp) || !parse_port(dpa, &dp)) {
        return reject("IP 或端口不合法");
    }
    if (strcmp(hex, "-") != 0 && !parse_hex(hex, data, UDP_DATA_MAX, &n)) {
        return reject("数据要是偶数字符的十六进制");
    }
    err = udp_set(&g, src, dst, sp, dp, n ? data : NULL, n);
    if (err != NULL) {
        return reject(err);
    }
    udp_show_build(&g);
    return 0;
}

static int load_gram(char *p, UdpGram *g, int need_ip) {
    char ipa[32];
    char ipb[32];
    char hex[(8 + UDP_DATA_MAX) * 2 + 1];
    unsigned char raw[8 + UDP_DATA_MAX];
    unsigned char src[4];
    unsigned char dst[4];
    int n = 0;
    const char *err;

    if (need_ip) {
        if (take_token(&p, ipa, (int)sizeof ipa) != 1 || take_token(&p, ipb, (int)sizeof ipb) != 1) {
            return reject("需要源 IP 和目的 IP");
        }
        if (!parse_ip(ipa, src) || !parse_ip(ipb, dst)) {
            return reject("IP 不合法");
        }
    }
    if (take_token(&p, hex, (int)sizeof hex) != 1 || !rest_blank(p)) {
        return reject("需要一份十六进制的 UDP 报文");
    }
    if (!parse_hex(hex, raw, (int)sizeof raw, &n)) {
        return reject("报文要是偶数字符的十六进制，最长 264 字节");
    }
    err = udp_read(raw, n, g);
    if (err != NULL) {
        return reject(err);
    }
    if (need_ip) {
        memcpy(g->src, src, 4);
        memcpy(g->dst, dst, 4);
    }
    return 0;
}

static int cmd_recv(char *p) {
    UdpGram g;

    if (load_gram(p, &g, 1) != 0) {
        return -1;
    }
    if (g.partial) {
        return reject("检验需要完整的 UDP 数据报，不能只有首部");
    }
    udp_show_recv(&g);
    return 0;
}

static int cmd_header(char *p) {
    UdpGram g;

    if (load_gram(p, &g, 0) != 0) {
        return -1;
    }
    udp_show_header(&g);
    return 0;
}

static int cmd_eff(char *p) {
    char tok[32];
    unsigned int app;

    if (take_token(&p, tok, (int)sizeof tok) != 1 || !parse_u32(tok, &app) || !rest_blank(p)) {
        return reject("eff 需要应用层字节数");
    }
    if (app == 0 || app > 65535u) {
        return reject("应用层字节数要在 1～65535");
    }
    udp_show_eff(app);
    return 0;
}

int run_udp(char *line) {
    char *p = line;
    char cmd[16];
    int got = take_token(&p, cmd, (int)sizeof cmd);

    if (got < 0) {
        return reject("命令太长");
    }
    if (strcmp(cmd, "sum") == 0) {
        return cmd_sum(p);
    }
    if (strcmp(cmd, "build") == 0) {
        return cmd_build(p);
    }
    if (strcmp(cmd, "recv") == 0) {
        return cmd_recv(p);
    }
    if (strcmp(cmd, "header") == 0) {
        return cmd_header(p);
    }
    if (strcmp(cmd, "demux") == 0) {
        return rest_blank(p) ? (udp_show_demux(), 0) : reject("demux 没有参数");
    }
    if (strcmp(cmd, "eff") == 0) {
        return cmd_eff(p);
    }
    return reject("未知命令");
}
