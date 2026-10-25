#include "run_hdr.h"

#include "cap.h"
#include "hdr.h"
#include "parse.h"
#include "scan.h"
#include "show.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int on;
    uint32_t host;
    uint32_t server;
    IpInfo pkt[CAP_MAX];
    int n;
    int has_origin;
    IpInfo origin;
} CapState;

static CapState cap;

int hdr_pending(void) {
    return cap.on;
}

static int load_header(const char **ps, IpInfo *ip, char *err, int err_cap) {
    unsigned char buf[IP_CAPTURE_MAX];
    int n = 0;

    if (!parse_hex_bytes(ps, buf, IP_CAPTURE_MAX, &n, err, err_cap)) {
        return 0;
    }
    return ip_parse_header(buf, n, ip, err, err_cap);
}

static int cmd_hdr(const char **ps, char *err, int err_cap) {
    IpInfo ip;

    if (cap.on) {
        return fail_msg(err, err_cap, "捕获还没 end");
    }
    if (!load_header(ps, &ip, err, err_cap)) {
        return 0;
    }
    show_ip(stdout, "IPv4 首部", &ip);
    return 1;
}

static int cmd_capture(const char **ps, char *err, int err_cap) {
    char tok[64];

    if (cap.on) {
        return fail_msg(err, err_cap, "上一次捕获还没 end");
    }
    memset(&cap, 0, sizeof cap);
    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !token_eq(tok, "host") ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_host_token(tok, &cap.host, err, err_cap) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !token_eq(tok, "server") ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_host_token(tok, &cap.server, err, err_cap) ||
        !rest_empty(*ps)) {
        return fail_msg(err, err_cap, "capture host <IP> server <IP>");
    }
    cap.on = 1;
    return 1;
}

static int cmd_pkt(const char **ps, char *err, int err_cap) {
    char title[32];

    if (!cap.on) {
        return fail_msg(err, err_cap, "先写 capture");
    }
    if (cap.n >= CAP_MAX) {
        return fail_msg(err, err_cap, "一组捕获最多 8 个分组");
    }
    if (!load_header(ps, &cap.pkt[cap.n], err, err_cap)) {
        return 0;
    }
    cap.n++;
    snprintf(title, sizeof title, "分组 %d", cap.n);
    show_ip(stdout, title, &cap.pkt[cap.n - 1]);
    return 1;
}

static int cmd_origin(const char **ps, char *err, int err_cap) {
    if (!cap.on) {
        return fail_msg(err, err_cap, "先写 capture");
    }
    if (cap.has_origin) {
        return fail_msg(err, err_cap, "发出时的分组已经有了");
    }
    if (!load_header(ps, &cap.origin, err, err_cap)) {
        return 0;
    }
    cap.has_origin = 1;
    show_ip(stdout, "发出时", &cap.origin);
    return 1;
}

static int cmd_end(const char **ps, char *err, int err_cap) {
    CapResult result;

    if (!cap.on) {
        return fail_msg(err, err_cap, "还没有 capture");
    }
    if (!rest_empty(*ps)) {
        return fail_msg(err, err_cap, "end 后面不要再写内容");
    }
    if (cap.n < 1) {
        return fail_msg(err, err_cap, "还没有分组");
    }
    if (!cap_conclude(cap.pkt, cap.n, cap.host, cap.server, cap.has_origin ? &cap.origin : NULL, &result, err,
                      err_cap)) {
        return 0;
    }
    show_cap(stdout, &result, cap.pkt, cap.host);
    memset(&cap, 0, sizeof cap);
    return 1;
}

int run_hdr(const char *line) {
    const char *p = line;
    char cmd[32];
    char err[160];
    int ok = 0;

    err[0] = '\0';
    if (!need_token(&p, cmd, (int)sizeof cmd, err, (int)sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    if (token_eq(cmd, "hdr")) {
        ok = cmd_hdr(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "capture")) {
        ok = cmd_capture(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "pkt")) {
        ok = cmd_pkt(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "origin")) {
        ok = cmd_origin(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "end")) {
        ok = cmd_end(&p, err, (int)sizeof err);
    } else {
        ok = fail_msg(err, (int)sizeof err, "不认识的命令");
    }
    if (!ok) {
        fprintf(stderr, "%s\n", err[0] ? err : "命令不完整");
        return 1;
    }
    return 0;
}
