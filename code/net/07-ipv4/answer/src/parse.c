#include "parse.h"

#include "scan.h"

#include <ctype.h>
#include <string.h>

int need_token(const char **ps, char *out, int cap, char *err, int err_cap) {
    int rc = take_token(ps, out, cap);

    if (rc == 0) {
        return fail_msg(err, err_cap, "参数不够");
    }
    if (rc < 0) {
        return fail_msg(err, err_cap, "词太长");
    }
    return 1;
}

int parse_host_token(const char *tok, uint32_t *ip, char *err, int err_cap) {
    if (!ip_parse(tok, ip)) {
        return fail_msg(err, err_cap, "IP 地址不合法");
    }
    return 1;
}

int parse_block_token(const char *tok, Block *b, char *err, int err_cap) {
    const char *slash = strchr(tok, '/');
    char ipbuf[32];
    unsigned pref = 0;
    int i;

    if (slash == NULL || slash == tok || (size_t)(slash - tok) >= sizeof ipbuf) {
        return fail_msg(err, err_cap, "要写成 地址/前缀");
    }
    memcpy(ipbuf, tok, (size_t)(slash - tok));
    ipbuf[slash - tok] = '\0';
    if (!ip_parse(ipbuf, &b->base) || slash[1] == '\0') {
        return fail_msg(err, err_cap, "地址或前缀不合法");
    }
    for (i = 1; slash[i] != '\0'; i++) {
        if (!isdigit((unsigned char)slash[i])) {
            return fail_msg(err, err_cap, "前缀不合法");
        }
    }
    if (!parse_u32(slash + 1, &pref) || pref > 32u) {
        return fail_msg(err, err_cap, "前缀要在 0 到 32");
    }
    b->prefix = (int)pref;
    return 1;
}

int parse_mask_token(const char *tok, int *prefix, char *err, int err_cap) {
    uint32_t mask;

    if (!ip_parse(tok, &mask)) {
        return fail_msg(err, err_cap, "掩码不合法");
    }
    if (!mask_to_prefix(mask, prefix)) {
        return fail_msg(err, err_cap, "掩码必须是连续的 1");
    }
    return 1;
}
