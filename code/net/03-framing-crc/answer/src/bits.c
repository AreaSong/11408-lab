#include "bits.h"

#include <stdio.h>
#include <string.h>

void lab_err(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
}

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int parse_bits(const char *text, BitStr *out, char *err, int err_cap) {
    int n = 0;

    out->len = 0;
    out->bit[0] = '\0';
    for (; *text != '\0'; text++) {
        if (is_space(*text)) {
            continue;
        }
        if (*text != '0' && *text != '1') {
            lab_err(err, err_cap, "比特串里只能有 0 和 1");
            return 0;
        }
        if (n >= LAB_BIT_MAX) {
            lab_err(err, err_cap, "比特串最长 256 位");
            return 0;
        }
        out->bit[n++] = *text;
    }
    out->bit[n] = '\0';
    out->len = n;
    return 1;
}

int parse_bytes(const char *text, ByteStr *out, char *err, int err_cap) {
    int n = 0;

    out->len = 0;
    while (*text != '\0') {
        int hi;
        int lo;

        while (is_space(*text)) {
            text++;
        }
        if (*text == '\0') {
            break;
        }
        hi = hex_value(*text++);
        lo = hex_value(*text);
        if (hi < 0 || lo < 0) {
            lab_err(err, err_cap, "字节要写成两位十六进制，例如 7E");
            return 0;
        }
        text++;
        if (n >= LAB_BYTE_MAX) {
            lab_err(err, err_cap, "字节最多 128 个");
            return 0;
        }
        out->byte[n++] = (unsigned char)((hi << 4) | lo);
    }
    out->len = n;
    return 1;
}

int format_bytes(const unsigned char *b, int n, char *out, int cap) {
    int used = 0;
    int i;

    if (cap <= 0) {
        return 0;
    }
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        int w = snprintf(out + used, (size_t)(cap - used), "%s%02X", i ? " " : "", b[i]);
        if (w < 0 || used + w >= cap) {
            return 0;
        }
        used += w;
    }
    return 1;
}
