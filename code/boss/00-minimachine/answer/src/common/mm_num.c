#include "common/mm_internal.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

void mm_err(char *err, size_t cap, const char *fmt, ...) {
    va_list ap;
    if (err == NULL || cap == 0) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(err, cap, fmt, ap);
    va_end(ap);
}

static int hex_value(char ch) {
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

static int parse_hex(const char *text, int64_t *out) {
    int64_t value = 0;
    if (*text == '\0') {
        return -1;
    }
    while (*text) {
        int digit = hex_value(*text);
        if (digit < 0 || value > (INT64_MAX >> 4)) {
            return -1;
        }
        value = (value << 4) | (int64_t)digit;
        text++;
    }
    *out = value;
    return 0;
}

static int parse_dec(const char *text, int64_t *out) {
    char *end = NULL;
    long long value;
    if (*text == '\0') {
        return -1;
    }
    errno = 0;
    value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return -1;
    }
    *out = (int64_t)value;
    return 0;
}

int mm_parse_i64(const char *text, int64_t *out, char *err, size_t cap) {
    int neg = 0;
    int64_t mag = 0;
    if (text[0] == '+' || text[0] == '-') {
        neg = text[0] == '-';
        text++;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        if (parse_hex(text + 2, &mag) != 0) {
            mm_err(err, cap, "非法十六进制数");
            return -1;
        }
    } else {
        size_t n = strlen(text);
        int hex = n > 1 && (text[n - 1] == 'h' || text[n - 1] == 'H');
        if (hex) {
            char buf[64];
            if (n >= sizeof buf) {
                mm_err(err, cap, "数字太长");
                return -1;
            }
            memcpy(buf, text, n - 1);
            buf[n - 1] = '\0';
            if (parse_hex(buf, &mag) != 0) {
                mm_err(err, cap, "非法十六进制数");
                return -1;
            }
        } else if (parse_dec(text, &mag) != 0) {
            mm_err(err, cap, "非法数字");
            return -1;
        }
    }
    if (neg) {
        if (mag > (int64_t)INT64_MAX) {
            mm_err(err, cap, "数字超出范围");
            return -1;
        }
        *out = -mag;
    } else {
        *out = mag;
    }
    return 0;
}
