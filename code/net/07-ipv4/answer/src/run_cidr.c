#include "run_cidr.h"

#include "parse.h"
#include "scan.h"
#include "show.h"

#include <stdio.h>
#include <string.h>

static int extra(const char *s, char *err, int err_cap) {
    if (!rest_empty(s)) {
        return fail_msg(err, err_cap, "多出来的内容");
    }
    return 1;
}

static int cmd_addr(const char **ps, char *err, int err_cap) {
    char tok[64];
    Block block;
    uint32_t ip;
    int prefix;

    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap)) {
        return 0;
    }
    if (strchr(tok, '/') != NULL) {
        if (!parse_block_token(tok, &block, err, err_cap) || !extra(*ps, err, err_cap)) {
            return 0;
        }
        show_addr(stdout, block.base, block.prefix);
        return 1;
    }
    if (!parse_host_token(tok, &ip, err, err_cap) || !need_token(ps, tok, (int)sizeof tok, err, err_cap)) {
        return 0;
    }
    if (!token_eq(tok, "mask")) {
        return fail_msg(err, err_cap, "addr 要写 地址/前缀，或 地址 mask 掩码");
    }
    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_mask_token(tok, &prefix, err, err_cap) ||
        !extra(*ps, err, err_cap)) {
        return 0;
    }
    show_addr(stdout, ip, prefix);
    return 1;
}

static int cmd_class(const char **ps, char *err, int err_cap) {
    char tok[64];
    uint32_t ip;

    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_host_token(tok, &ip, err, err_cap) ||
        !extra(*ps, err, err_cap)) {
        return 0;
    }
    show_class(stdout, ip);
    return 1;
}

static int cmd_subnet(const char **ps, char *err, int err_cap) {
    char tok[64];
    Block base;
    unsigned hosts = 0;
    unsigned count = 0;
    int prefix;
    EqualSplit equal;
    MaskSplit mask;

    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_block_token(tok, &base, err, err_cap) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap)) {
        return 0;
    }
    if (token_eq(tok, "hosts")) {
        if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_u32(tok, &hosts) || hosts == 0 ||
            !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !token_eq(tok, "count") ||
            !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_u32(tok, &count) ||
            !extra(*ps, err, err_cap)) {
            return fail_msg(err, err_cap, "subnet 要写 hosts <个数> count <个数>");
        }
        if (!subnet_equal(base, hosts, (int)count, &equal, err, err_cap)) {
            return 0;
        }
        show_equal(stdout, base, hosts, &equal);
        return 1;
    }
    if (!token_eq(tok, "mask")) {
        return fail_msg(err, err_cap, "subnet 后面要写 hosts 或 mask");
    }
    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_mask_token(tok, &prefix, err, err_cap) ||
        !extra(*ps, err, err_cap) || !subnet_by_mask(base, prefix, &mask, err, err_cap)) {
        return 0;
    }
    show_mask(stdout, base, &mask);
    return 1;
}

static int cmd_split(const char **ps, char *err, int err_cap) {
    char tok[64];
    Block base;
    unsigned parts = 0;
    VlsmSplit split;

    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_block_token(tok, &base, err, err_cap) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap)) {
        return 0;
    }
    if (!token_eq(tok, "into")) {
        return fail_msg(err, err_cap, "split 要写 地址/前缀 into <块数>");
    }
    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_u32(tok, &parts)) {
        return fail_msg(err, err_cap, "split 要写 into <块数>");
    }
    if (!extra(*ps, err, err_cap) || !split_min(base, (int)parts, &split, err, err_cap)) {
        return 0;
    }
    show_vlsm(stdout, base, &split);
    return 1;
}

static int read_blocks(const char **ps, Block *out, int cap, int *n, char *err, int err_cap) {
    *n = 0;
    while (!rest_empty(*ps)) {
        char tok[64];
        if (*n >= cap || !need_token(ps, tok, (int)sizeof tok, err, err_cap) ||
            !parse_block_token(tok, &out[*n], err, err_cap)) {
            return *n >= cap ? fail_msg(err, err_cap, "块太多") : 0;
        }
        (*n)++;
    }
    return 1;
}

static int cmd_aggregate(const char **ps, char *err, int err_cap) {
    char tok[64];
    Block item[BLOCK_LIST];
    int n = 0;
    int has_via = 0;
    uint32_t via = 0;
    AggResult agg;

    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap)) {
        return 0;
    }
    if (token_eq(tok, "via")) {
        if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_host_token(tok, &via, err, err_cap) ||
            !need_token(ps, tok, (int)sizeof tok, err, err_cap)) {
            return 0;
        }
        has_via = 1;
    }
    if (!parse_block_token(tok, &item[0], err, err_cap)) {
        return 0;
    }
    n = 1;
    if (!read_blocks(ps, item + 1, BLOCK_LIST - 1, &n, err, err_cap)) {
        return 0;
    }
    n += 1;
    if (!aggregate_blocks(item, n, &agg, err, err_cap)) {
        return 0;
    }
    show_agg(stdout, item, n, &agg, has_via, via);
    return 1;
}

static int cmd_siblings(const char **ps, char *err, int err_cap) {
    char tok[64];
    unsigned target = 0;
    Block known;
    Block cand;
    SiblingResult result;

    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_u32(tok, &target) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_block_token(tok, &known, err, err_cap) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_block_token(tok, &cand, err, err_cap) ||
        !extra(*ps, err, err_cap) || !sibling_blocks(known, cand, (int)target, &result, err, err_cap)) {
        return 0;
    }
    show_sibling(stdout, known, cand, (int)target, &result);
    return 1;
}

static int cmd_remain(const char **ps, char *err, int err_cap) {
    char tok[64];
    Block net;
    uint32_t from;
    uint32_t to;
    uint32_t router;
    unsigned used = 0;
    uint64_t left = 0;

    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_block_token(tok, &net, err, err_cap) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !token_eq(tok, "from") ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_host_token(tok, &from, err, err_cap) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !token_eq(tok, "to") ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_host_token(tok, &to, err, err_cap) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !token_eq(tok, "router") ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_host_token(tok, &router, err, err_cap) ||
        !extra(*ps, err, err_cap) || !hosts_left(net, from, to, router, &used, &left, err, err_cap)) {
        return 0;
    }
    show_remain(stdout, net, from, to, router, used, left);
    return 1;
}

int run_cidr(const char *line) {
    const char *p = line;
    char cmd[32];
    char err[160];
    int ok;

    err[0] = '\0';
    if (!need_token(&p, cmd, (int)sizeof cmd, err, (int)sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    if (token_eq(cmd, "addr")) {
        ok = cmd_addr(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "class")) {
        ok = cmd_class(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "subnet")) {
        ok = cmd_subnet(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "split")) {
        ok = cmd_split(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "aggregate")) {
        ok = cmd_aggregate(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "siblings")) {
        ok = cmd_siblings(&p, err, (int)sizeof err);
    } else if (token_eq(cmd, "remain")) {
        ok = cmd_remain(&p, err, (int)sizeof err);
    } else {
        ok = fail_msg(err, (int)sizeof err, "不认识的命令");
    }
    if (!ok) {
        if (err[0] == '\0') {
            fprintf(stderr, "命令不完整\n");
        } else {
            fprintf(stderr, "%s\n", err);
        }
        return 1;
    }
    return 0;
}
