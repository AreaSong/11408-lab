#include "run_frag.h"

#include "frag.h"
#include "parse.h"
#include "scan.h"
#include "show.h"

#include <stdio.h>
#include <string.h>

static int take_num(const char **ps, const char *unit, unsigned *out, char *err, int err_cap) {
    char tok[32];

    if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_u32(tok, out) ||
        !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !token_eq(tok, unit)) {
        return fail_msg(err, err_cap, "数值后面要写单位 B");
    }
    return 1;
}

static int take_optional(const char **ps, unsigned *id, int *has_id, int *df, char *err, int err_cap) {
    while (!rest_empty(*ps)) {
        char tok[32];
        unsigned value = 0;

        if (!need_token(ps, tok, (int)sizeof tok, err, err_cap)) {
            return 0;
        }
        if (token_eq(tok, "id")) {
            if (*has_id || !need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_u32(tok, &value) ||
                value > 65535u) {
                return fail_msg(err, err_cap, "id 要是 0 到 65535，并且只写一次");
            }
            *id = value;
            *has_id = 1;
        } else if (token_eq(tok, "df")) {
            if (!need_token(ps, tok, (int)sizeof tok, err, err_cap) || !parse_u32(tok, &value) || value > 1u) {
                return fail_msg(err, err_cap, "df 只能是 0 或 1");
            }
            *df = (int)value;
        } else {
            return fail_msg(err, err_cap, "frag 可选的只有 id 和 df");
        }
    }
    return 1;
}

int run_frag(const char *line) {
    const char *p = line;
    char tok[32];
    char err[160];
    unsigned total = 0;
    unsigned header = 0;
    unsigned mtu = 0;
    unsigned id = 0;
    int has_id = 0;
    int df = 0;
    FragPlan plan;

    err[0] = '\0';
    if (!need_token(&p, tok, (int)sizeof tok, err, (int)sizeof err) || !token_eq(tok, "frag") ||
        !need_token(&p, tok, (int)sizeof tok, err, (int)sizeof err) || !token_eq(tok, "total") ||
        !take_num(&p, "B", &total, err, (int)sizeof err) ||
        !need_token(&p, tok, (int)sizeof tok, err, (int)sizeof err) || !token_eq(tok, "header") ||
        !take_num(&p, "B", &header, err, (int)sizeof err) ||
        !need_token(&p, tok, (int)sizeof tok, err, (int)sizeof err) || !token_eq(tok, "mtu") ||
        !take_num(&p, "B", &mtu, err, (int)sizeof err) || !take_optional(&p, &id, &has_id, &df, err, (int)sizeof err) ||
        !frag_plan((int)total, (int)header, (int)mtu, df, &plan, err, (int)sizeof err)) {
        fprintf(stderr, "%s\n", err[0] ? err : "frag total <长度> B header <长度> B mtu <长度> B");
        return 1;
    }
    show_frag(stdout, (int)total, (int)header, (int)mtu, df, id, has_id, &plan);
    return 0;
}
