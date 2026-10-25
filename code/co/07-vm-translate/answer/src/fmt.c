#include "fmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fmt_parse(const char *text, uint64_t *out) {
    char buf[64];
    char *end = NULL;
    unsigned long long value;
    size_t n;
    if (text == NULL || text[0] == '\0' || text[0] == '-') {
        return 0;
    }
    n = strlen(text);
    if (n >= sizeof buf) {
        return 0;
    }
    if (n > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        value = strtoull(text, &end, 0);
    } else if (n > 1 && (text[n - 1] == 'H' || text[n - 1] == 'h')) {
        memcpy(buf, text, n - 1);
        buf[n - 1] = '\0';
        value = strtoull(buf, &end, 16);
    } else {
        value = strtoull(text, &end, 10);
    }
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

int fmt_hex(char *dst, size_t n, uint64_t value, int bits) {
    int width;
    if (bits <= 0) {
        return snprintf(dst, n, "0x%llX", (unsigned long long)value) > 0 ? 0 : 1;
    }
    width = (bits + 3) / 4;
    if (width < 1) {
        width = 1;
    }
    if (width > 16) {
        width = 16;
    }
    return snprintf(dst, n, "0x%0*llX", width, (unsigned long long)value) > 0 ? 0 : 1;
}

int fmt_h(char *dst, size_t n, uint64_t value, int bits) {
    int width;
    if (bits <= 0) {
        return snprintf(dst, n, "%llXH", (unsigned long long)value) > 0 ? 0 : 1;
    }
    width = (bits + 3) / 4;
    if (width < 1) {
        width = 1;
    }
    if (width > 16) {
        width = 16;
    }
    return snprintf(dst, n, "%0*llXH", width, (unsigned long long)value) > 0 ? 0 : 1;
}

int fmt_bin(char *dst, size_t n, uint64_t value, int bits) {
    int i;
    if (bits < 1 || (size_t)bits + 2 > n) {
        return 1;
    }
    for (i = 0; i < bits; i++) {
        uint64_t bit = (value >> (bits - 1 - i)) & 1ull;
        dst[i] = bit ? '1' : '0';
    }
    dst[bits] = 'B';
    dst[bits + 1] = '\0';
    return 0;
}
