#ifndef SHOW_UTIL_H
#define SHOW_UTIL_H

#include "fp.h"

#include <stdio.h>
#include <string.h>

static inline void write_bits(uint64_t value, int width, char *buf) {
    int i;
    for (i = 0; i < width; i++) {
        buf[i] = ((value >> (width - 1 - i)) & 1ULL) ? '1' : '0';
    }
    buf[width] = '\0';
}

static inline int text_is_hex(const char *text) {
    size_t n = strlen(text);
    if (n >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        return 1;
    }
    return n > 0 && (text[n - 1] == 'H' || text[n - 1] == 'h');
}

static inline int load_float(const char *text, uint32_t *bits, char *err, size_t n) {
    if (text_is_hex(text)) {
        uint64_t value = 0;
        if (!hex_parse(text, 8, &value, err, n)) {
            return 0;
        }
        *bits = (uint32_t)value;
        return 1;
    }
    {
        Decimal dec;
        EncShow show;
        if (!dec_parse(text, &dec, err, n) || !fp_encode(&dec, &FMT32, &show, err, n)) {
            return 0;
        }
        *bits = (uint32_t)show.bits;
        return 1;
    }
}

static inline void value_text(uint32_t bits, char *buf, size_t n) {
    View view = fp_view(bits, &FMT32);
    char exam[80];
    char dec[80];
    fp_exam(&view, &FMT32, exam, sizeof exam);
    if (fp_decimal(&view, &FMT32, dec, sizeof dec)) {
        snprintf(buf, n, "%s", dec);
        return;
    }
    snprintf(buf, n, "%s", exam);
}

#endif
