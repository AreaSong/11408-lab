/*
 * Cache 平均访问时间和效率。考纲把命中率落到时间上，王道有两套公式，差在缺失时
 * 查 Cache 的时间算不算进去：先查 Cache 再查主存要算，两边同时启动就不算。
 * 效率 e = tc / t，看平均时间离 Cache 还有多远。
 */
#include "amat.h"

#include "fields.h"

#include <stdio.h>
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

static void reduce(long long *num, long long *den) {
    long long g = igcd(*num, *den);
    if (*den < 0) {
        *num = -*num;
        *den = -*den;
    }
    *num /= g;
    *den /= g;
}

int amat_make(Amat *out, long long tc, long long tm, long long accesses, long long misses) {
    long long hits;
    if (out == NULL || tc <= 0 || tm <= 0 || accesses <= 0 || misses < 0 || misses > accesses) {
        return 1;
    }
    /* 1e9 以内，乘积进得了 64 位，考题的 ns 和次数都远小于这个上界。 */
    if (tc > 1000000000LL || tm > 1000000000LL || accesses > 1000000000LL) {
        return 1;
    }
    hits = accesses - misses;
    memset(out, 0, sizeof(*out));
    /* 先访问 Cache：每次都付 tc，缺失再付 tm。 */
    out->seq_num = accesses * tc + misses * tm;
    out->seq_den = accesses;
    /* 同时访问：命中只付 tc，缺失只付 tm。 */
    out->par_num = hits * tc + misses * tm;
    out->par_den = accesses;
    reduce(&out->seq_num, &out->seq_den);
    reduce(&out->par_num, &out->par_den);
    out->seq_e_num = tc * out->seq_den;
    out->seq_e_den = out->seq_num;
    out->par_e_num = tc * out->par_den;
    out->par_e_den = out->par_num;
    reduce(&out->seq_e_num, &out->seq_e_den);
    reduce(&out->par_e_num, &out->par_e_den);
    return 0;
}

/* 能除尽就写成整数或最多三位小数，否则保留已约分的分数。 */
static void fmt_ns(char *buf, size_t n, long long num, long long den) {
    int places;
    long long scale = 1;
    if (den == 1) {
        snprintf(buf, n, "%lld ns", num);
        return;
    }
    for (places = 1; places <= 3; places++) {
        long long scaled;
        long long whole;
        int frac;
        if (scale > 922337203685477580LL || num > 922337203685477580LL / (scale * 10)) {
            break;
        }
        scale *= 10;
        if (num * scale % den != 0) {
            continue;
        }
        scaled = num * scale / den;
        whole = scaled / scale;
        frac = (int)(scaled % scale);
        if (places == 1) {
            snprintf(buf, n, "%lld.%d ns", whole, frac);
        } else if (places == 2) {
            snprintf(buf, n, "%lld.%02d ns", whole, frac);
        } else {
            snprintf(buf, n, "%lld.%03d ns", whole, frac);
        }
        return;
    }
    snprintf(buf, n, "%lld/%lld ns", num, den);
}

static void fmt_eff(char *buf, size_t n, long long num, long long den) {
    char pct[32];
    if (num >= 0 && den > 0 && num <= 2147483647LL && den <= 2147483647LL) {
        format_percent(pct, sizeof pct, (int)num, (int)den);
        if (strchr(pct, '%') != NULL) {
            snprintf(buf, n, "%s", pct);
            return;
        }
    }
    snprintf(buf, n, "%lld/%lld", num, den);
}

static void print_one(FILE *out, const char *when, const char *formula, const char *hit, const char *miss,
                      long long tc, long long extra, const char *time, long long tc_ns, const char *eff) {
    fprintf(out, "%s\n", when);
    fprintf(out, "%s\n", formula);
    fprintf(out, "t = %s×%lld ns + %s×%lld ns = %s\n", hit, tc, miss, extra, time);
    fprintf(out, "效率 e = tc / t = %lld ns / %s = %s\n", tc_ns, time, eff);
}

int show_amat(FILE *out, long long tc, long long tm, long long accesses, long long misses) {
    Amat a;
    char hit[32];
    char miss[32];
    char seq_t[48];
    char par_t[48];
    char seq_e[32];
    char par_e[32];
    long long hits;
    if (amat_make(&a, tc, tm, accesses, misses) != 0) {
        fprintf(stderr, "amat 需要正的 tc、tm、访存次数，缺失次数不能超过访存次数\n");
        return 1;
    }
    hits = accesses - misses;
    format_percent(hit, sizeof hit, (int)hits, (int)accesses);
    format_percent(miss, sizeof miss, (int)misses, (int)accesses);
    fmt_ns(seq_t, sizeof seq_t, a.seq_num, a.seq_den);
    fmt_ns(par_t, sizeof par_t, a.par_num, a.par_den);
    fmt_eff(seq_e, sizeof seq_e, a.seq_e_num, a.seq_e_den);
    fmt_eff(par_e, sizeof par_e, a.par_e_num, a.par_e_den);
    fprintf(out, "【平均访问时间与效率】\n");
    fprintf(out, "tc = %lld ns，tm = %lld ns\n", tc, tm);
    fprintf(out, "访存 %lld 次，缺失 %lld 次，命中 %lld 次\n", accesses, misses, hits);
    fprintf(out, "命中率 h = %s，缺失率 = %s\n", hit, miss);
    print_one(out, "先访问 Cache，未命中再访问主存。缺失时 tc 已经花掉，还要再花 tm",
              "t = h×tc + (1-h)×(tc+tm)", hit, miss, tc, tc + tm, seq_t, tc, seq_e);
    print_one(out, "Cache 与主存同时访问。命中就放弃主存，缺失只计 tm",
              "t = h×tc + (1-h)×tm", hit, miss, tc, tm, par_t, tc, par_e);
    fprintf(out, "两种口径差在缺失时要不要把已经花掉的 tc 再算一次。题面写「先访问 Cache」用前一种，写「同时访问」用后一种\n");
    fprintf(out, "结果：先访问 Cache 的 t = %s，e = %s；同时访问的 t = %s，e = %s\n", seq_t, seq_e, par_t,
            par_e);
    return 0;
}
