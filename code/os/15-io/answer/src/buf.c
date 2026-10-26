#include "buf.h"

#include <stdio.h>

/*
 * 缓冲区在搬到用户区结束时就空了，CPU 还在用户区里算。
 * 单缓冲只有一块，下一次输入必须等这次搬运结束。
 * 双缓冲两块交替，下一次输入可以打进另一块。
 * 用户区同一时刻只放一块，所以下一次搬运还要等上一块 CPU 算完。
 */
long long buf_finish(int dual, int n, long long t, long long m, long long c, BufStep *out) {
    long long dev = 0;
    long long user = 0;
    long long free_at[2] = {0, 0};
    int i;

    if (n < 1) {
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
        if (out != 0) {
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

/* 稳态下多处理一块的增量。第一块单独是 T+M+C，所以总数要补上这一段。 */
long long buf_closed(int dual, int n, long long t, long long m, long long c) {
    long long io = c > t ? c : t;
    long long step = dual ? (m + c > t ? m + c : t) : io + m;

    if (n < 1) {
        return -1;
    }
    return t + m + c + (long long)(n - 1) * step;
}

static int report_one(int dual, int trace, int n, long long t, long long m, long long c) {
    BufStep steps[16];
    long long sim;
    long long closed;
    long long avg;
    int i;

    sim = buf_finish(dual, n, t, m, c, trace ? steps : 0);
    closed = buf_closed(dual, n, t, m, c);
    avg = buf_closed(dual, 2, t, m, c) - buf_closed(dual, 1, t, m, c);
    printf("%s avg=%lld total=%lld sim=%lld\n", dual ? "double" : "single", avg, closed, sim);
    if (!trace) {
        return sim == closed ? 0 : -1;
    }
    for (i = 0; i < n; i++) {
        printf("block=%d buf=%d in=%lld-%lld move=%lld-%lld cpu=%lld-%lld\n", steps[i].block,
               steps[i].buf, steps[i].in0, steps[i].in1, steps[i].mv0, steps[i].mv1,
               steps[i].cpu0, steps[i].cpu1);
    }
    return sim == closed ? 0 : -1;
}

int buf_report(int which, int trace, int n, long long t, long long m, long long c) {
    int d;

    if (trace && (n < 1 || n > 16)) {
        return -1;
    }
    for (d = 0; d < 2; d++) {
        if (which != 2 && which != d) {
            continue;
        }
        if (report_one(d, trace, n, t, m, c) != 0) {
            return -1;
        }
    }
    return 0;
}
