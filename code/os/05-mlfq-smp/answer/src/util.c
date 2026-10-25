#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int util_split(const char *line, char out[][32], int max) {
    int n = 0;
    const char *p = line;
    while (*p != '\0') {
        int k = 0;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (n >= max) {
            return -2;
        }
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            if (k >= 31) {
                return -1;
            }
            out[n][k++] = *p++;
        }
        out[n][k] = '\0';
        n++;
    }
    return n;
}

int util_ll(const char *text, long long *out) {
    char *end = NULL;
    long long value;
    errno = 0;
    value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return -1;
    }
    *out = value;
    return 0;
}

int util_name_ok(const char *text) {
    int n = 0;
    if (text == NULL || !isalpha((unsigned char)text[0])) {
        return 0;
    }
    for (; text[n] != '\0'; n++) {
        unsigned char c = (unsigned char)text[n];
        if (!isalnum(c) && c != '_') {
            return 0;
        }
    }
    if (n > 15) {
        return 0;
    }
    /* idle 留给闲逛进程，end 是名单结束符。 */
    if (strcmp(text, "end") == 0 || strcmp(text, "idle") == 0) {
        return 0;
    }
    return 1;
}

long long util_gcd(long long a, long long b) {
    if (a < 0) {
        a = -a;
    }
    if (b < 0) {
        b = -b;
    }
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

void util_print_frac(long long n, long long d) {
    long long g;
    if (d < 0) {
        n = -n;
        d = -d;
    }
    if (d == 0) {
        d = 1;
    }
    g = util_gcd(n, d);
    printf("%lld/%lld", n / g, d / g);
}
