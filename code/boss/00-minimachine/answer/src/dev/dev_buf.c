#include "dev/dev_buf.h"

#include <stddef.h>

long long dev_buf_finish(int dual, int n, long long t, long long m, long long c, DevBufStep *out) {
    long long dev = 0;
    long long user = 0;
    long long free_at[2] = {0, 0};
    int i;
    if (n < 1 || t < 0 || m < 0 || c < 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        int b = dual ? (i & 1) : 0;
        long long in0 = dev > free_at[b] ? dev : free_at[b];
        long long in1 = in0 + t;
        long long mv0 = in1 > user ? in1 : user;
        long long mv1 = mv0 + m;
        long long cpu1 = mv1 + c;
        dev = in1;
        user = cpu1;
        free_at[b] = mv1;
        if (out != NULL) {
            out[i].block = i + 1;
            out[i].buf = b;
            out[i].in0 = in0;
            out[i].in1 = in1;
            out[i].mv0 = mv0;
            out[i].mv1 = mv1;
            out[i].cpu0 = mv1;
            out[i].cpu1 = cpu1;
        }
    }
    return user;
}

long long dev_buf_closed(int dual, int n, long long t, long long m, long long c) {
    long long io = c > t ? c : t;
    long long step = dual ? (m + c > t ? m + c : t) : io + m;
    if (n < 1 || t < 0 || m < 0 || c < 0) {
        return -1;
    }
    return t + m + c + (long long)(n - 1) * step;
}
