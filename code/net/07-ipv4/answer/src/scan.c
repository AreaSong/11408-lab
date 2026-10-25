#include "scan.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int fail_msg(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return 0;
}

const char *skip_space(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }
    return s;
}

int rest_empty(const char *s) {
    return *skip_space(s) == '\0';
}

int take_token(const char **ps, char *out, int cap) {
    const char *s = skip_space(*ps);
    int n = 0;

    if (*s == '\0') {
        return 0;
    }
    while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') {
        if (n + 1 >= cap) {
            return -1;
        }
        out[n++] = *s++;
    }
    out[n] = '\0';
    *ps = s;
    return 1;
}

int token_eq(const char *tok, const char *lit) {
    return strcmp(tok, lit) == 0;
}

int parse_u32(const char *tok, unsigned *out) {
    unsigned v = 0;
    int i;

    if (tok[0] == '\0') {
        return 0;
    }
    for (i = 0; tok[i] != '\0'; i++) {
        if (!isdigit((unsigned char)tok[i])) {
            return 0;
        }
        if (v > 429496729u || (v == 429496729u && tok[i] > '5')) {
            return 0;
        }
        v = v * 10u + (unsigned)(tok[i] - '0');
    }
    *out = v;
    return 1;
}

static int hex_nibble(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

int parse_hex_bytes(const char **ps, unsigned char *buf, int cap, int *n,
                    char *err, int err_cap) {
    char tok[16];
    int got = 0;

    while (1) {
        int rc = take_token(ps, tok, (int)sizeof tok);
        int hi;
        int lo;

        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            return fail_msg(err, err_cap, "这一行太长");
        }
        if (strlen(tok) != 2) {
            return fail_msg(err, err_cap, "每个字节要写成两位十六进制");
        }
        hi = hex_nibble(tok[0]);
        lo = hex_nibble(tok[1]);
        if (hi < 0 || lo < 0) {
            return fail_msg(err, err_cap, "十六进制里有不认识的字符");
        }
        if (got >= cap) {
            return fail_msg(err, err_cap, "字节太多");
        }
        buf[got++] = (unsigned char)((hi << 4) | lo);
    }
    if (got == 0) {
        return fail_msg(err, err_cap, "缺少十六进制字节");
    }
    *n = got;
    return 1;
}
