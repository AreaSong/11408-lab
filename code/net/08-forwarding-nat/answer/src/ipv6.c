#include "ipv6.h"

#include <stdio.h>
#include <string.h>

static int hex_group(const char *s, int n, unsigned *out) {
    unsigned value = 0;
    int i;

    if (n < 1 || n > 4) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        char c = s[i];
        unsigned digit;

        if (c >= '0' && c <= '9') {
            digit = (unsigned)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = (unsigned)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = (unsigned)(c - 'A' + 10);
        } else {
            return 0;
        }
        value = (value << 4) | digit;
    }
    *out = value;
    return 1;
}

static int parse_side(const char *s, int n, unsigned *out) {
    int count = 0;
    int i = 0;

    if (n == 0) {
        return 0;
    }
    while (i < n) {
        int start = i;

        while (i < n && s[i] != ':') {
            i++;
        }
        if (i == start || count == 8 || !hex_group(s + start, i - start, &out[count])) {
            return -1;
        }
        count++;
        if (i == n) {
            break;
        }
        i++;
        if (i == n) {
            return -1;
        }
    }
    return count;
}

static int read_side(const char *s, int n, unsigned *out, int *count) {
    int got;

    if (n == 0) {
        *count = 0;
        return 1;
    }
    got = parse_side(s, n, out);
    if (got < 0) {
        return 0;
    }
    *count = got;
    return 1;
}

int ipv6_parse(const char *text, uint16_t group[8]) {
    const char *gap;
    unsigned left[8];
    unsigned right[8];
    int left_n = 0;
    int right_n = 0;
    int zeros;
    int i;
    int k = 0;
    size_t n = strlen(text);

    if (n == 0 || n > 63) {
        return 0;
    }
    gap = strstr(text, "::");
    if (gap == NULL) {
        if (!read_side(text, (int)n, left, &left_n) || left_n != 8) {
            return 0;
        }
        for (i = 0; i < 8; i++) {
            group[i] = (uint16_t)left[i];
        }
        return 1;
    }
    if (strstr(gap + 2, "::") != NULL) {
        return 0;
    }
    if (!read_side(text, (int)(gap - text), left, &left_n)) {
        return 0;
    }
    if (!read_side(gap + 2, (int)strlen(gap + 2), right, &right_n)) {
        return 0;
    }
    zeros = 8 - left_n - right_n;
    if (zeros < 1) {
        return 0;
    }
    for (i = 0; i < left_n; i++) {
        group[k++] = (uint16_t)left[i];
    }
    for (i = 0; i < zeros; i++) {
        group[k++] = 0;
    }
    for (i = 0; i < right_n; i++) {
        group[k++] = (uint16_t)right[i];
    }
    return k == 8;
}

void ipv6_zero_span(const uint16_t group[8], ZeroSpan *span) {
    int i = 0;

    span->start = -1;
    span->len = 0;
    span->ties = 0;
    while (i < 8) {
        int n = 0;

        if (group[i] != 0) {
            i++;
            continue;
        }
        while (i + n < 8 && group[i + n] == 0) {
            n++;
        }
        if (n > span->len) {
            span->len = n;
            span->start = i;
            span->ties = 1;
        } else if (n == span->len) {
            span->ties++;
        }
        i += n;
    }
}

int ipv6_expand(const uint16_t group[8], char *out, int cap) {
    int wrote = snprintf(out, (size_t)cap, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", group[0], group[1],
                         group[2], group[3], group[4], group[5], group[6], group[7]);

    return wrote > 0 && wrote < cap;
}

int ipv6_compress(const uint16_t group[8], char *out, int cap) {
    ZeroSpan span;
    int i = 0;
    int pos = 0;

    ipv6_zero_span(group, &span);
    if (span.len < 2) {
        span.start = -1;
    }
    if (span.len == 8) {
        return snprintf(out, (size_t)cap, "::") > 0;
    }
    out[0] = '\0';
    while (i < 8) {
        int wrote;
        int pair;

        if (i == span.start) {
            wrote = snprintf(out + pos, (size_t)(cap - pos), "::");
            if (wrote < 0 || wrote >= cap - pos) {
                return 0;
            }
            pos += wrote;
            i += span.len;
            continue;
        }
        pair = pos >= 2 && out[pos - 1] == ':' && out[pos - 2] == ':';
        wrote = snprintf(out + pos, (size_t)(cap - pos), (pos == 0 || pair) ? "%x" : ":%x", group[i]);
        if (wrote < 0 || wrote >= cap - pos) {
            return 0;
        }
        pos += wrote;
        i++;
    }
    return 1;
}
