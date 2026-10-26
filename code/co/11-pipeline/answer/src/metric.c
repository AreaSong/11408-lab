#include "metric.h"

#include <string.h>

static long long gcd_ll(long long a, long long b) {
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
    long long g = gcd_ll(*num, *den);
    *num /= g;
    *den /= g;
}

int metric_cycles(int k, int n) {
    return k + n - 1;
}

void metric_ratio(int k, int n, long long *s_num, long long *s_den, long long *e_num,
                  long long *e_den) {
    *s_num = (long long)n * k;
    *s_den = metric_cycles(k, n);
    *e_num = n;
    *e_den = metric_cycles(k, n);
    reduce(s_num, s_den);
    reduce(e_num, e_den);
}

long long metric_period(const long long *stage, int nstage, long long latch) {
    long long slow = stage[0];
    int i;
    for (i = 1; i < nstage; i++) {
        if (stage[i] > slow) {
            slow = stage[i];
        }
    }
    return slow + latch;
}

long long metric_tp(int k, int n, int mhz) {
    long long den = metric_cycles(k, n);
    long long num = (long long)mhz * n * 1000000LL;
    if (num % den != 0) {
        return -1;
    }
    return num / den;
}

static int cycle_unit(const char *unit) {
    return strcmp(unit, "cycle") == 0;
}

static void print_span(FILE *fp, long long ticks, long long dt, const char *unit) {
    if (cycle_unit(unit)) {
        fprintf(fp, "%lld 个时钟周期", ticks * dt);
        return;
    }
    fprintf(fp, "%lld %s", ticks * dt, unit);
}

static void print_one(FILE *fp, long long dt, const char *unit) {
    if (cycle_unit(unit)) {
        fprintf(fp, "%lld 个时钟周期", dt);
        return;
    }
    fprintf(fp, "%lld %s", dt, unit);
}

static const char *stage_name(int k, int index) {
    static const char *five[] = {"IF", "ID", "EX", "MEM", "WB"};
    static char buf[8];
    if (k == 5) {
        return five[index];
    }
    snprintf(buf, sizeof buf, "S%d", index + 1);
    return buf;
}

static void print_ideal_chart(FILE *fp, int k, int n) {
    int cycles = metric_cycles(k, n);
    int c;
    int i;
    fprintf(fp, "时空图\n时钟");
    for (c = 1; c <= cycles; c++) {
        fprintf(fp, "%5d", c);
    }
    fputc('\n', fp);
    for (i = 0; i < n; i++) {
        fprintf(fp, "I%-3d", i + 1);
        for (c = 1; c <= cycles; c++) {
            int seg = c - 1 - i;
            const char *name = (seg >= 0 && seg < k) ? stage_name(k, seg) : "";
            fprintf(fp, "%5s", name);
        }
        fputc('\n', fp);
    }
}

void metric_ideal(FILE *fp, int k, int n, long long dt, const char *unit) {
    int cycles = metric_cycles(k, n);
    long long sn;
    long long sd;
    long long en;
    long long ed;
    metric_ratio(k, n, &sn, &sd, &en, &ed);
    fprintf(fp, "【理想流水线】\n");
    fprintf(fp, "k = %d 段，n = %d 条，Δt = ", k, n);
    print_one(fp, dt, unit);
    fputc('\n', fp);
    fprintf(fp, "各段时间相等，连续流入，没有冒险\n");
    fprintf(fp, "流水线时间 Tk = (%d + %d - 1) × ", k, n);
    print_one(fp, dt, unit);
    fprintf(fp, " = ");
    print_span(fp, cycles, dt, unit);
    fputc('\n', fp);
    fprintf(fp, "顺序执行 T0 = %d × %d × ", n, k);
    print_one(fp, dt, unit);
    fprintf(fp, " = ");
    print_span(fp, (long long)n * k, dt, unit);
    fputc('\n', fp);
    fprintf(fp, "加速比 S = T0 / Tk = %lld/%lld\n", sn, sd);
    fprintf(fp, "吞吐率 TP = %d 条 / ", n);
    print_span(fp, cycles, dt, unit);
    fputc('\n', fp);
    fprintf(fp, "最大吞吐率 TPmax = 1 / ");
    print_one(fp, dt, unit);
    fputc('\n', fp);
    fprintf(fp, "效率 E = 有效格 / 总格 = (%d × %d) / (%d × %d) = %lld/%lld\n", n, k, k,
            cycles, en, ed);
    fprintf(fp, "n 很大时加速比接近 %d，效率接近 1\n", k);
    if (n <= 6 && k <= 5) {
        print_ideal_chart(fp, k, n);
    } else {
        fprintf(fp, "指令较多，时空图省略\n");
    }
    fprintf(fp, "结果：加速比 %lld/%lld，吞吐率 %d 条/", sn, sd, n);
    print_span(fp, cycles, dt, unit);
    fprintf(fp, "，效率 %lld/%lld\n", en, ed);
}

static void print_rate(FILE *fp, long long per_s) {
    if (per_s % 1000000000LL == 0) {
        fprintf(fp, "%lld.0×10^9 条/秒", per_s / 1000000000LL);
        return;
    }
    if (per_s % 10000000LL == 0) {
        long long x = per_s / 10000000LL;
        fprintf(fp, "%lld.%02lld×10^9 条/秒", x / 100, x % 100);
        return;
    }
    fprintf(fp, "%lld 条/秒", per_s);
}

void metric_freq(FILE *fp, int k, int n, int mhz) {
    int cycles = metric_cycles(k, n);
    long long per_s = metric_tp(k, n, mhz);
    fprintf(fp, "【吞吐率】\n");
    fprintf(fp, "k = %d 段，n = %d 条，没有阻塞\n", k, n);
    fprintf(fp, "时钟周期数 = %d + %d - 1 = %d 个\n", k, n, cycles);
    fprintf(fp, "主频 = %d MHz = %d.%02d GHz = ", mhz, mhz / 1000, (mhz % 1000) / 10);
    if (mhz % 10 == 0) {
        fprintf(fp, "%d.%02d×10^9 Hz\n", mhz / 1000, (mhz % 1000) / 10);
    } else {
        fprintf(fp, "%d MHz\n", mhz);
    }
    fprintf(fp, "吞吐率 = 主频 × %d / %d = ", n, cycles);
    if (per_s < 0) {
        fprintf(fp, "除不尽\n结果：吞吐率 除不尽\n");
        return;
    }
    print_rate(fp, per_s);
    fputc('\n', fp);
    fprintf(fp, "结果：吞吐率 ");
    print_rate(fp, per_s);
    fputc('\n', fp);
}

void metric_clock(FILE *fp, const long long *stage, int nstage, long long latch,
                  const char *unit) {
    long long slow = stage[0];
    long long period;
    int i;
    fprintf(fp, "【时钟周期】\n各段");
    for (i = 0; i < nstage; i++) {
        if (stage[i] > slow) {
            slow = stage[i];
        }
        fprintf(fp, "%s%lld %s", i == 0 ? " " : "、", stage[i], unit);
    }
    fputc('\n', fp);
    period = slow + latch;
    fprintf(fp, "段间锁存 %lld %s\n", latch, unit);
    fprintf(fp, "同步流水线的时钟必须盖住最长的那一段，再加流水段寄存器的延迟\n");
    fprintf(fp, "时钟周期 = %lld %s + %lld %s = %lld %s\n", slow, unit, latch, unit, period,
            unit);
    fprintf(fp, "最大吞吐率 = 1 / %lld %s\n", period, unit);
    fprintf(fp, "结果：时钟周期 %lld %s\n", period, unit);
}
