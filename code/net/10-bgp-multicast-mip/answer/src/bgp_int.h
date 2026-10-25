#ifndef BGP_INT_H
#define BGP_INT_H

#include "bgp.h"

#include <stdarg.h>
#include <stdio.h>

static inline void say(FILE *fp, const char *fmt, ...) {
    va_list ap;

    if (fp == NULL) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
}

static inline void path_text(const int *path, int n, char *out, int cap) {
    int i;
    int used = 0;

    if (n <= 0) {
        snprintf(out, (size_t)cap, "（空）");
        return;
    }
    out[0] = '\0';
    for (i = 0; i < n && used < cap; i++) {
        int k = snprintf(out + used, (size_t)(cap - used), "%s%d", i ? " " : "", path[i]);
        if (k < 0 || used + k >= cap) {
            return;
        }
        used += k;
    }
}

#endif
