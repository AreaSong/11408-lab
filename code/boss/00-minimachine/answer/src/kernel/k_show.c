#include "kernel/k_i.h"

#include <string.h>

static long long igcd(long long a, long long b) {
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

static void norm(long long *n, long long *d) {
    long long g;
    if (*d < 0) {
        *n = -*n;
        *d = -*d;
    }
    if (*d == 0) {
        *d = 1;
    }
    g = igcd(*n, *d);
    *n /= g;
    *d /= g;
}

static void frac_add(long long *n, long long *d, long long a, long long b) {
    long long nn = *n * b + a * *d;
    long long dd = *d * b;
    *n = nn;
    *d = dd;
    norm(n, d);
}

static void frac_div(long long *n, long long *d, int k) {
    *d *= k;
    norm(n, d);
}

void k_avg_fill(KSchedOut *out, const long long *cpu) {
    long long tn = 0;
    long long td = 1;
    long long wn = 0;
    long long wd = 1;
    long long qn = 0;
    long long qd = 1;
    int i;
    for (i = 0; i < out->n; i++) {
        long long den = cpu[i] > 0 ? cpu[i] : 1;
        frac_add(&tn, &td, out->turn[i], 1);
        frac_add(&qn, &qd, out->wait[i], 1);
        frac_add(&wn, &wd, out->turn[i], den);
    }
    frac_div(&tn, &td, out->n);
    frac_div(&qn, &qd, out->n);
    frac_div(&wn, &wd, out->n);
    out->avg_turn_n = tn;
    out->avg_turn_d = td;
    out->avg_wait_n = qn;
    out->avg_wait_d = qd;
    out->avg_weight_n = wn;
    out->avg_weight_d = wd;
}

static void add_seg(KSchedOut *out, long long t0, long long t1, const char *name) {
    int n;
    if (t0 == t1 || out->error[0] != '\0') {
        return;
    }
    n = out->nseg;
    if (n > 0 && out->seg_t1[n - 1] == t0 && strcmp(out->seg_name[n - 1], name) == 0) {
        out->seg_t1[n - 1] = t1;
        return;
    }
    if (n >= K_SEG_MAX) {
        snprintf(out->error, sizeof out->error, "too-long");
        return;
    }
    out->seg_t0[n] = t0;
    out->seg_t1[n] = t1;
    snprintf(out->seg_name[n], sizeof out->seg_name[n], "%s", name);
    out->nseg = n + 1;
}

void k_sched_print(const KSchedOut *out, FILE *fp) {
    int i;
    if (out->error[0] != '\0') {
        fprintf(fp, "错误：%s\n", out->error);
        return;
    }
    fprintf(fp, "algo=%s\n", out->algo);
    fprintf(fp, "gantt\n");
    for (i = 0; i < out->nseg; i++) {
        fprintf(fp, "%lld-%lld %s\n", out->seg_t0[i], out->seg_t1[i], out->seg_name[i]);
    }
    fprintf(fp, "avg turn=%lld/%lld weight=%lld/%lld wait=%lld/%lld\n", out->avg_turn_n, out->avg_turn_d,
            out->avg_weight_n, out->avg_weight_d, out->avg_wait_n, out->avg_wait_d);
}

/* 给 k_sched.c / k_mlfq.c 用，避免两边各写一份合并。 */
void k_seg_add(KSchedOut *out, long long t0, long long t1, const char *name) {
    add_seg(out, t0, t1, name);
}
