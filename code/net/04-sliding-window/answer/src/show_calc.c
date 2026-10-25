#include "show_calc.h"

#include "limit.h"
#include "qty.h"

#include <limits.h>
#include <stdio.h>

static void fmt_ratio(char *buf, size_t n, long long num, long long den) {
    long long scaled;
    long long ip;
    long long fp;
    char frac[8];
    int len;

    if (den <= 0 || num < 0 || den > LLONG_MAX / 2 || num > (LLONG_MAX - den / 2) / 10000) {
        snprintf(buf, n, "?");
        return;
    }
    scaled = (num * 10000 + den / 2) / den;
    ip = scaled / 10000;
    fp = scaled % 10000;
    if (fp == 0) {
        snprintf(buf, n, "%lld", ip);
        return;
    }
    snprintf(frac, sizeof frac, "%04lld", fp);
    len = 4;
    while (len > 1 && frac[len - 1] == '0') {
        frac[--len] = '\0';
    }
    snprintf(buf, n, "%lld.%s", ip, frac);
}

void fmt_milli(char *buf, size_t n, int milli, const char *unit) {
    char frac[8];
    int ip = milli / 1000;
    int fp = milli % 1000;
    int len;
    const char *gap = unit[0] == '%' ? "" : " ";

    if (fp == 0) {
        snprintf(buf, n, "%d%s%s", ip, gap, unit);
        return;
    }
    snprintf(frac, sizeof frac, "%03d", fp);
    len = 3;
    while (len > 1 && frac[len - 1] == '0') {
        frac[--len] = '\0';
    }
    snprintf(buf, n, "%d.%s%s%s", ip, frac, gap, unit);
}

void fmt_short_rate(char *buf, size_t n, long long bps) {
    if (bps % 1000000000LL == 0 && bps != 0) {
        snprintf(buf, n, "%lld Gbps", bps / 1000000000LL);
    } else if (bps % 1000000LL == 0 && bps != 0) {
        snprintf(buf, n, "%lld Mbps", bps / 1000000LL);
    } else if (bps % 1000 == 0 && bps != 0) {
        snprintf(buf, n, "%lld kbps", bps / 1000);
    } else {
        snprintf(buf, n, "%lld bps", bps);
    }
}

void print_timing(const PipeDelay *d, int ack_same) {
    char frame[128], ack[128], rate[96], prop[80], td[80], ta[80], rtt[80], cycle[80];
    char td1[48], ta1[48], rtt1[48], cycle1[48];

    fmt_bits(frame, sizeof frame, d->frame_bits);
    fmt_bits(ack, sizeof ack, d->ack_bits);
    fmt_bps(rate, sizeof rate, d->bps);
    fmt_time(prop, sizeof prop, d->prop_ns);
    fmt_time(td, sizeof td, d->td_ns);
    fmt_time(ta, sizeof ta, d->ta_ns);
    fmt_time(rtt, sizeof rtt, d->prop_ns * 2);
    fmt_time(cycle, sizeof cycle, d->cycle_ns);
    fmt_time_one(td1, sizeof td1, d->td_ns);
    fmt_time_one(ta1, sizeof ta1, d->ta_ns);
    fmt_time_one(rtt1, sizeof rtt1, d->prop_ns * 2);
    fmt_time_one(cycle1, sizeof cycle1, d->cycle_ns);
    qty_print_scale_note(stdout);
    printf("数据帧 = %s\n", frame);
    printf(ack_same ? "确认帧与数据帧等长 = %s\n" : "确认帧 = %s\n", ack);
    printf("带宽 R = %s\n", rate);
    printf("单向传播时延 = %s\n", prop);
    printf("数据帧发送时延 Td = %lld bit / %lld bps = %s\n", d->frame_bits, d->bps, td);
    printf("确认帧发送时延 Ta = %lld bit / %lld bps = %s\n", d->ack_bits, d->bps, ta);
    printf("往返传播 = 2 × 单向传播 = %s\n", rtt);
    printf("周期 = Td + Ta + 往返传播 = %s + %s + %s = %s\n", td1, ta1, rtt1, cycle1);
}

void print_window_need(const PipeDelay *d, int percent, int window) {
    char ratio[32];
    int full = pipe_window_for(d, 100);

    fmt_ratio(ratio, sizeof ratio, d->cycle_ns * percent, d->td_ns * 100);
    printf("目标利用率 %d%%。W × Td / 周期 >= %d%%，所以 W >= %s。\n", percent, percent, ratio);
    printf("不够整帧的部分也要占一个窗口位置，发送窗口至少 %d。\n", window);
    if (percent < 100 && full > window) {
        printf("要完全用满，窗口得至少 %d。这一问只要 %d%%。\n", full, percent);
    }
}

static void print_gbn_bits(int window) {
    int n = limit_bits_gbn(window);

    printf("GBN：2^n - 1 >= %d。", window);
    if (n > 1) {
        printf("n = %d 时最大窗口 %d，不够；", n - 1, limit_max_gbn(n - 1));
    }
    printf("n = %d 时最大窗口 %d，够。\n", n, limit_max_gbn(n));
}

static void print_sr_bits(int window) {
    int n = limit_bits_sr_equal(window);

    printf("SR 收发窗口相等：2^(n-1) >= %d。", window);
    if (n > 1) {
        printf("n = %d 时最大 %d，不够；", n - 1, limit_max_sr_equal(n - 1));
    }
    printf("n = %d 时最大 %d，够。\n", n, limit_max_sr_equal(n));
}

void print_bits(int kind, int window) {
    if (kind == 0 && window > 1) {
        printf("停等的窗口只能是 1，这里至少要 %d，停等达不到这个利用率。\n", window);
        return;
    }
    if (kind == 0) {
        puts("停等窗口是 1，1 位序号就够。");
        return;
    }
    if (kind == 1) {
        print_gbn_bits(window);
        return;
    }
    if (kind == 2) {
        print_sr_bits(window);
        return;
    }
    puts("题面没有点名 GBN 还是 SR，两种都要装得下这个窗口。");
    print_gbn_bits(window);
    print_sr_bits(window);
    printf("更紧的那个是 %d 位。\n", limit_bits_slide(window));
}
