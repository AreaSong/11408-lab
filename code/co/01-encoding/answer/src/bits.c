#include "bits.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

unsigned long long bits_mask(int width) {
    if (width <= 0) {
        return 0;
    }
    if (width >= 64) {
        return ~0ULL;
    }
    return (1ULL << width) - 1ULL;
}

bool bits_width_ok(int width) {
    return width >= 1 && width <= 64;
}

Bits bits_make(int width, unsigned long long raw) {
    Bits b;
    b.width = width;
    b.bits = raw & bits_mask(width);
    return b;
}

static void set_err(char *err, size_t n, const char *msg) {
    if (err != NULL && n > 0) {
        snprintf(err, n, "%s", msg);
    }
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/* 去掉空白、0x 前缀和末尾的 H，结果放进 dst。返回 false 表示空串。 */
static bool strip_hex_text(const char *text, char *dst, size_t n) {
    size_t k = 0;
    const char *p = text;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    while (*p != '\0' && !isspace((unsigned char)*p)) {
        if (k + 1 >= n) {
            return false;
        }
        dst[k++] = *p++;
    }
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '\0' || k == 0) {
        return false;
    }
    if (dst[k - 1] == 'H' || dst[k - 1] == 'h') {
        dst[--k] = '\0';
    } else {
        dst[k] = '\0';
    }
    return k > 0;
}

bool bits_parse_hex(const char *text, unsigned long long *out, char *err, size_t errn) {
    char buf[80];
    unsigned long long value = 0;
    size_t i;
    if (!strip_hex_text(text, buf, sizeof buf)) {
        set_err(err, errn, "十六进制写不成（可用 0x 前缀或 H 后缀）");
        return false;
    }
    for (i = 0; buf[i] != '\0'; i++) {
        int d = hex_value(buf[i]);
        if (d < 0) {
            set_err(err, errn, "十六进制里有不认识的字符");
            return false;
        }
        if (value > (~0ULL >> 4)) {
            set_err(err, errn, "十六进制超过 64 位");
            return false;
        }
        value = (value << 4) | (unsigned)d;
    }
    *out = value;
    return true;
}

static bool only_bits(const char *s) {
    size_t i;
    if (s[0] == '\0') {
        return false;
    }
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] != '0' && s[i] != '1') {
            return false;
        }
    }
    return true;
}

bool bits_parse(const char *text, int width, Bits *out, char *err, size_t errn) {
    char buf[80];
    unsigned long long value = 0;
    size_t i;
    size_t len;
    if (!bits_width_ok(width)) {
        set_err(err, errn, "位宽要在 1 到 64 之间");
        return false;
    }
    if (!strip_hex_text(text, buf, sizeof buf)) {
        set_err(err, errn, "机器数是空的");
        return false;
    }
    /* 纯 0/1 且没有 H：按二进制，必须写满字长，避免把 10 读成十六进制的 16 */
    if (only_bits(buf) && strchr(text, 'H') == NULL && strchr(text, 'h') == NULL &&
        strstr(text, "0x") == NULL && strstr(text, "0X") == NULL) {
        len = strlen(buf);
        if ((int)len != width) {
            snprintf(err, errn, "二进制要写满 %d 位，现在是 %zu 位", width, len);
            return false;
        }
        for (i = 0; i < len; i++) {
            value = (value << 1) | (unsigned)(buf[i] - '0');
        }
        *out = bits_make(width, value);
        return true;
    }
    if (!bits_parse_hex(text, &value, err, errn)) {
        return false;
    }
    if (width < 64 && value > bits_mask(width)) {
        snprintf(err, errn, "数值超出 %d 位", width);
        return false;
    }
    *out = bits_make(width, value);
    return true;
}

void bits_write_bin(char *dst, size_t n, Bits b) {
    int i;
    size_t k = 0;
    for (i = b.width - 1; i >= 0 && k + 1 < n; i--) {
        dst[k++] = ((b.bits >> i) & 1ULL) ? '1' : '0';
    }
    if (n > 0) {
        dst[k < n ? k : n - 1] = '\0';
    }
}

void bits_write_point(char *dst, size_t n, Bits b) {
    char bin[80];
    bits_write_bin(bin, sizeof bin, b);
    if (b.width <= 1) {
        snprintf(dst, n, "%s", bin);
        return;
    }
    snprintf(dst, n, "%c.%s", bin[0], bin + 1);
}

void bits_write_hex(char *dst, size_t n, Bits b) {
    int digits = (b.width + 3) / 4;
    int i;
    size_t k = 0;
    if (digits < 1) {
        digits = 1;
    }
    for (i = digits - 1; i >= 0 && k + 1 < n; i--) {
        int nibble = (int)((b.bits >> (i * 4)) & 0xF);
        dst[k++] = "0123456789ABCDEF"[nibble];
        if (i > 0 && i % 4 == 0 && k + 1 < n) {
            dst[k++] = ' ';
        }
    }
    if (k + 2 < n) {
        dst[k++] = 'H';
        dst[k] = '\0';
    } else if (n > 0) {
        dst[n - 1] = '\0';
    }
}

void bits_write_addr(char *dst, size_t n, unsigned long long addr) {
    snprintf(dst, n, "%08llXH", addr);
}
