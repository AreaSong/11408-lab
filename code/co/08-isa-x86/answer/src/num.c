#include "num.h"

#include <ctype.h>
#include <string.h>

static int parse_digits(const char *text, int base, uint64_t *out) {
    uint64_t value = 0;
    if (text[0] == '\0') {
        return -1;
    }
    for (; *text != '\0'; text++) {
        int digit;
        if (*text >= '0' && *text <= '9') {
            digit = *text - '0';
        } else if (*text >= 'a' && *text <= 'f') {
            digit = *text - 'a' + 10;
        } else if (*text >= 'A' && *text <= 'F') {
            digit = *text - 'A' + 10;
        } else {
            return -1;
        }
        if (digit >= base || value > (~0ULL - (uint64_t)digit) / (uint64_t)base) {
            return -1;
        }
        value = value * (uint64_t)base + (uint64_t)digit;
    }
    *out = value;
    return 0;
}

int num_parse(const char *text, int64_t *out) {
    int neg = 0;
    uint64_t mag = 0;
    size_t len;
    if (text[0] == '+') {
        text++;
    } else if (text[0] == '-') {
        neg = 1;
        text++;
    }
    len = strlen(text);
    if (len >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        if (parse_digits(text + 2, 16, &mag) != 0) {
            return -1;
        }
    } else if (len >= 2 && (text[len - 1] == 'H' || text[len - 1] == 'h')) {
        char buf[80];
        if (len - 1 >= sizeof buf) {
            return -1;
        }
        memcpy(buf, text, len - 1);
        buf[len - 1] = '\0';
        if (parse_digits(buf, 16, &mag) != 0) {
            return -1;
        }
    } else if (parse_digits(text, 10, &mag) != 0) {
        return -1;
    }
    if (!neg) {
        if (mag > (uint64_t)INT64_MAX) {
            return -1;
        }
        *out = (int64_t)mag;
        return 0;
    }
    if (mag > (uint64_t)INT64_MAX + 1ULL) {
        return -1;
    }
    *out = mag == (uint64_t)INT64_MAX + 1ULL ? INT64_MIN : -(int64_t)mag;
    return 0;
}

int num_pow2(int bits, uint64_t *out) {
    if (bits < 0 || bits > 62) {
        return -1;
    }
    *out = 1ULL << bits;
    return 0;
}

int num_ceil_log2(uint64_t n, int *bits) {
    uint64_t power = 1;
    int width = 0;
    if (n == 0) {
        return -1;
    }
    while (power < n) {
        if (width >= 62) {
            return -1;
        }
        power <<= 1;
        width++;
    }
    *bits = width;
    return 0;
}

int64_t num_sext(uint64_t bits, int width) {
    uint64_t mask;
    uint64_t low;
    if (width <= 0 || width > 64) {
        return 0;
    }
    if (width == 64) {
        return (int64_t)bits;
    }
    mask = (1ULL << width) - 1ULL;
    low = bits & mask;
    if ((low & (1ULL << (width - 1))) == 0) {
        return (int64_t)low;
    }
    /* 先减 2^width，避免把最高位为 1 的位模式直接转成有符号数。 */
    return (int64_t)low - (int64_t)(1ULL << width);
}

void num_print_addr(FILE *out, uint64_t value) {
    int digits = 1;
    uint64_t rest = value;
    while (rest > 0xF) {
        digits++;
        rest >>= 4;
    }
    if (value > 0xFFFF && digits < 8) {
        digits = 8;
    }
    fprintf(out, "0x%0*llX（%0*llXH）", digits, (unsigned long long)value, digits, (unsigned long long)value);
}

int num_split(char *line, char **tok, int max) {
    int n = 0;
    char *p = line;
    while (*p != '\0' && n < max) {
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        tok[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        *p++ = '\0';
    }
    return n;
}
