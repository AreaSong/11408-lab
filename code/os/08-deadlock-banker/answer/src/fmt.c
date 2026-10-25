#include "fmt.h"

#include <stdio.h>

int fmt_vec(char *dst, int cap, const int *v, int m) {
    int i;
    int used = 0;
    if (cap <= 0) {
        return -1;
    }
    dst[0] = '\0';
    for (i = 0; i < m; i++) {
        int n = snprintf(dst + used, (size_t)(cap - used), "%s%d", i ? "," : "", v[i]);
        if (n < 0 || used + n >= cap) {
            return -1;
        }
        used += n;
    }
    return used;
}

int fmt_name(char *dst, int cap, int index, int base) {
    int n = snprintf(dst, (size_t)cap, "P%d", index + base);
    if (n < 0 || n >= cap) {
        return -1;
    }
    return n;
}

int fmt_names(char *dst, int cap, const int *idx, int n, int base) {
    int i;
    int used = 0;
    if (cap <= 0) {
        return -1;
    }
    if (n <= 0) {
        int wr = snprintf(dst, (size_t)cap, "-");
        return (wr < 0 || wr >= cap) ? -1 : 0;
    }
    dst[0] = '\0';
    for (i = 0; i < n; i++) {
        char one[16];
        int kn;
        int wr;
        if (fmt_name(one, (int)sizeof one, idx[i], base) < 0) {
            return -1;
        }
        kn = snprintf(dst + used, (size_t)(cap - used), "%s%s", i ? " " : "", one);
        wr = used + kn;
        if (kn < 0 || wr >= cap) {
            return -1;
        }
        used = wr;
    }
    return used;
}
