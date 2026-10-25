#include "word.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long long word_mask(int width) {
    if (width <= 0) {
        return 0;
    }
    if (width >= 64) {
        return ~0ULL;
    }
    return (1ULL << width) - 1ULL;
}

bool word_width_ok(int width) {
    return width >= 2 && width <= 32;
}

int word_bit(Word w, int index) {
    return (int)((w.bits >> index) & 1ULL);
}

Word word_make(int width, unsigned long long raw) {
    Word w;
    w.width = width;
    w.bits = raw & word_mask(width);
    return w;
}

long long word_smin(int width) {
    return -(1LL << (width - 1));
}

long long word_smax(int width) {
    return (1LL << (width - 1)) - 1LL;
}

long long word_signed(Word w) {
    unsigned long long sign = 1ULL << (w.width - 1);
    if ((w.bits & sign) == 0) {
        return (long long)w.bits;
    }
    return (long long)w.bits - (long long)(1ULL << w.width);
}

unsigned long long word_unsigned(Word w) {
    return w.bits;
}

static void set_err(char *err, size_t n, const char *msg) {
    if (err != NULL && n > 0) {
        snprintf(err, n, "%s", msg);
    }
}

static bool parse_u64(const char *text, int base, unsigned long long *out) {
    char *end = NULL;
    unsigned long long value;
    if (text[0] == '\0' || text[0] == '+' || text[0] == '-') {
        return false;
    }
    errno = 0;
    value = strtoull(text, &end, base);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_i64_local(const char *text, long long *out) {
    char *end = NULL;
    long long value;
    if (text[0] == '\0') {
        return false;
    }
    errno = 0;
    value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_bin(const char *text, int width, Word *out, char *err, size_t errn) {
    unsigned long long value = 0;
    int i;
    if ((int)strlen(text) != width) {
        set_err(err, errn, "0b 后面要刚好写满位宽");
        return false;
    }
    for (i = 0; text[i] != '\0'; i++) {
        if (text[i] != '0' && text[i] != '1') {
            set_err(err, errn, "二进制里只能有 0 和 1");
            return false;
        }
        value = (value << 1) | (unsigned)(text[i] - '0');
    }
    *out = word_make(width, value);
    return true;
}

static bool parse_hex(char *text, int width, Word *out, char *err, size_t errn) {
    unsigned long long value = 0;
    size_t n = strlen(text);
    char *body = text;
    if (body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) {
        body += 2;
    }
    n = strlen(body);
    if (n > 0 && (body[n - 1] == 'H' || body[n - 1] == 'h')) {
        body[n - 1] = '\0';
    }
    if (!parse_u64(body, 16, &value)) {
        set_err(err, errn, "十六进制写不成（可用 0x 前缀或 H 后缀）");
        return false;
    }
    if (value > word_mask(width)) {
        set_err(err, errn, "机器数超出位宽");
        return false;
    }
    *out = word_make(width, value);
    return true;
}

static bool parse_unsigned(const char *text, int width, Word *out, char *err, size_t errn) {
    unsigned long long value = 0;
    if (!parse_u64(text, 10, &value)) {
        set_err(err, errn, "无符号真值要写成不带符号的十进制，末尾加 u");
        return false;
    }
    if (value > word_mask(width)) {
        set_err(err, errn, "无符号真值超出位宽");
        return false;
    }
    *out = word_make(width, value);
    return true;
}

static bool parse_signed(const char *text, int width, Word *out, char *err, size_t errn) {
    long long value = 0;
    if (!parse_i64_local(text, &value)) {
        set_err(err, errn, "有符号真值要写成十进制");
        return false;
    }
    if (value < word_smin(width) || value > word_smax(width)) {
        set_err(err, errn, "这个真值超出该位宽的补码范围，无符号数请在末尾加 u");
        return false;
    }
    *out = word_make(width, (unsigned long long)value);
    return true;
}

static bool looks_hex(const char *text) {
    size_t n = strlen(text);
    if (n >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        return true;
    }
    return n > 0 && (text[n - 1] == 'H' || text[n - 1] == 'h');
}

bool word_parse(const char *text, int width, Word *out, char *err, size_t errn) {
    char buf[80];
    size_t n;
    if (!word_width_ok(width)) {
        set_err(err, errn, "位宽要在 2 到 32 之间");
        return false;
    }
    n = strlen(text);
    if (n == 0 || n >= sizeof buf) {
        set_err(err, errn, "这个数空着，或写得太长");
        return false;
    }
    memcpy(buf, text, n + 1);
    if (buf[0] == '0' && (buf[1] == 'b' || buf[1] == 'B')) {
        return parse_bin(buf + 2, width, out, err, errn);
    }
    if (looks_hex(buf)) {
        return parse_hex(buf, width, out, err, errn);
    }
    if (buf[n - 1] == 'u' || buf[n - 1] == 'U') {
        buf[n - 1] = '\0';
        return parse_unsigned(buf, width, out, err, errn);
    }
    if (buf[n - 1] == 's' || buf[n - 1] == 'S') {
        buf[n - 1] = '\0';
    }
    return parse_signed(buf, width, out, err, errn);
}

void word_write_bin(char *dst, size_t n, Word w) {
    int i;
    if (n < (size_t)w.width + 1) {
        if (n > 0) {
            dst[0] = '\0';
        }
        return;
    }
    for (i = 0; i < w.width; i++) {
        int bit = word_bit(w, w.width - 1 - i);
        dst[i] = bit ? '1' : '0';
    }
    dst[w.width] = '\0';
}

void word_write_hex(char *dst, size_t n, Word w) {
    static const char *digits = "0123456789ABCDEF";
    int count = (w.width + 3) / 4;
    int i;
    size_t k = 0;
    if (n == 0) {
        return;
    }
    for (i = count - 1; i >= 0; i--) {
        int digit = (int)((w.bits >> (i * 4)) & 0xF);
        int need_space = (i + 1) % 4 == 0 && i != count - 1;
        if (k + (size_t)need_space + 2 >= n) {
            dst[0] = '\0';
            return;
        }
        if (need_space) {
            dst[k++] = ' ';
        }
        dst[k++] = digits[digit];
    }
    dst[k++] = 'H';
    dst[k] = '\0';
}
