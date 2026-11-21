#include "net/net_v6.h"

#include <stdio.h>
#include <string.h>

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

static int read_group(const char **text, int *value) {
    int n = 0;
    int acc = 0;
    int digit;
    const char *p = *text;
    while ((digit = hex_value(*p)) >= 0) {
        acc = (acc << 4) + digit;
        p++;
        if (++n > 4 || acc > 0xffff) {
            return -1;
        }
    }
    if (n == 0) {
        return -1;
    }
    *value = acc;
    *text = p;
    return 0;
}

static int fill_gap(int group[8], const int *left, int ln, const int *right, int rn) {
    int zeros = 8 - ln - rn;
    if (zeros < 1) {
        return -1;
    }
    memcpy(group, left, (size_t)ln * sizeof(int));
    memset(group + ln, 0, (size_t)zeros * sizeof(int));
    memcpy(group + 8 - rn, right, (size_t)rn * sizeof(int));
    return 0;
}

static int parse_v6(const char *text, int group[8]) {
    int left[8];
    int right[8];
    int ln = 0;
    int rn = 0;
    int gap = 0;
    int *cur = left;
    int *count = &ln;
    const char *p = text;
    if (text == NULL || text[0] == '\0') {
        return -1;
    }
    while (*p != '\0') {
        if (p[0] == ':' && p[1] == ':') {
            if (gap) {
                return -1;
            }
            gap = 1;
            cur = right;
            count = &rn;
            p += 2;
            continue;
        }
        if (*p == ':' || *count >= 8 || read_group(&p, &cur[*count]) != 0) {
            return -1;
        }
        (*count)++;
        if (*p == ':' && p[1] != ':') {
            p++;
            if (*p == '\0') {
                return -1;
            }
        }
    }
    if (!gap) {
        if (ln != 8) {
            return -1;
        }
        memcpy(group, left, sizeof left);
        return 0;
    }
    return fill_gap(group, left, ln, right, rn);
}

static int zero_run(const int group[8], int *at) {
    int best = 0;
    int where = -1;
    int i = 0;
    while (i < 8) {
        int start;
        if (group[i] != 0) {
            i++;
            continue;
        }
        start = i;
        while (i < 8 && group[i] == 0) {
            i++;
        }
        if (i - start > best) {
            best = i - start;
            where = start;
        }
    }
    *at = best >= 2 ? where : -1;
    return best >= 2 ? best : 0;
}

static int append_group(char *out, int cap, int used, int value, int colon) {
    int n = snprintf(out + used, (size_t)cap - (size_t)used, "%s%x", colon ? ":" : "", value);
    if (n < 0 || used + n >= cap) {
        return -1;
    }
    return used + n;
}

int net_v6_compress(const char *text, char *out, int cap) {
    int group[8];
    int at = -1;
    int len;
    int i = 0;
    int used = 0;
    if (out == NULL || cap < 4 || parse_v6(text, group) != 0) {
        return -1;
    }
    len = zero_run(group, &at);
    if (len == 8) {
        snprintf(out, (size_t)cap, "::");
        return 0;
    }
    out[0] = '\0';
    while (i < 8) {
        if (i == at) {
            int n = snprintf(out + used, (size_t)cap - (size_t)used, "::");
            if (n < 0 || used + n >= cap) {
                return -1;
            }
            used += n;
            i += len;
            continue;
        }
        used = append_group(out, cap, used, group[i], i > 0 && i != at + len);
        if (used < 0) {
            return -1;
        }
        i++;
    }
    return 0;
}

int net_v6_expand(const char *text, char *out, int cap) {
    int group[8];
    int i;
    int used = 0;
    if (out == NULL || parse_v6(text, group) != 0) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        int n = snprintf(out + used, (size_t)cap - (size_t)used, "%s%04x", i ? ":" : "", group[i]);
        if (n < 0 || used + n >= cap) {
            return -1;
        }
        used += n;
    }
    return 0;
}
