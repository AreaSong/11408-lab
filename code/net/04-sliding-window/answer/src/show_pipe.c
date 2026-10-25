#include "show_pipe.h"

#include "limit.h"
#include "pipe.h"
#include "qty.h"
#include "scan.h"
#include "show_calc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXW 24

typedef struct {
    char w[MAXW][32];
    int n;
    int i;
} Line;

typedef struct {
    PipeDelay delay;
    int window;
    int bits;
    int ack_same;
} NeedCase;

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int split_line(const char *line, Line *a) {
    const char *p = line;
    int rc;

    a->n = 0;
    a->i = 0;
    while (!scan_blank(p)) {
        if (a->n >= MAXW) {
            return fail("词太多");
        }
        rc = scan_word(&p, a->w[a->n], 32);
        if (rc < 0) {
            return fail("这个词太长");
        }
        a->n++;
    }
    return 0;
}

static int parse_int(const char *s, int *out) {
    long v = 0;
    int i;

    if (s == NULL || s[0] == '\0') {
        return 0;
    }
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9' || v > 100000) {
            return 0;
        }
        v = v * 10 + (s[i] - '0');
    }
    *out = (int)v;
    return 1;
}

static int pull(Line *a, const char **out) {
    if (a->i >= a->n) {
        return 0;
    }
    *out = a->w[a->i++];
    return 1;
}

static int want(Line *a, const char *lit) {
    const char *s;

    return pull(a, &s) && strcmp(s, lit) == 0;
}

static int amount(Line *a, int kind, long long *out) {
    const char *num;
    const char *unit;
    char *end = NULL;
    double v;

    if (!pull(a, &num) || !pull(a, &unit)) {
        return 0;
    }
    v = strtod(num, &end);
    if (end == num || *end != '\0') {
        return 0;
    }
    if (kind == 0) {
        return parse_bits(v, unit, out);
    }
    if (kind == 1) {
        return parse_bps(v, unit, out);
    }
    return parse_ns(v, unit, out);
}

static int parse_ack(Line *a, long long *bits, int *same) {
    const char *s;
    const char *unit;
    char *end = NULL;
    double v;

    if (!want(a, "ack") || !pull(a, &s)) {
        return 0;
    }
    if (strcmp(s, "same") == 0) {
        *same = 1;
        *bits = 0;
        return 1;
    }
    v = strtod(s, &end);
    if (end == s || *end != '\0' || !pull(a, &unit)) {
        return 0;
    }
    *same = 0;
    return parse_bits(v, unit, bits);
}

static int parse_util(Line *a, int *percent) {
    const char *s;

    *percent = 100;
    if (a->i >= a->n) {
        return 1;
    }
    if (!want(a, "util") || !pull(a, &s) || !parse_int(s, percent)) {
        return 0;
    }
    return *percent >= 1 && *percent <= 100 && a->i == a->n;
}

static int parse_kind(const char *s, int *kind) {
    if (strcmp(s, "stop") == 0) {
        *kind = 0;
    } else if (strcmp(s, "gbn") == 0) {
        *kind = 1;
    } else if (strcmp(s, "sr") == 0) {
        *kind = 2;
    } else if (strcmp(s, "slide") == 0) {
        *kind = 3;
    } else {
        return 0;
    }
    return 1;
}

static const char *kind_name(int kind) {
    static const char *names[] = {"停等", "GBN", "SR", "滑动窗口"};

    return names[kind];
}

static int bits_of(int kind, int window) {
    if (window < 1) {
        return 0;
    }
    if (kind == 0) {
        return window == 1 ? 1 : 0;
    }
    if (kind == 1) {
        return limit_bits_gbn(window);
    }
    if (kind == 2) {
        return limit_bits_sr_equal(window);
    }
    return limit_bits_slide(window);
}

static int build_case(long long frame, long long ack, int same, long long bps, long long prop,
                      int percent, int kind, NeedCase *c) {
    char err[180];

    if (!pipe_delays(frame, ack, same, bps, prop, &c->delay, err, (int)sizeof err)) {
        return fail(err);
    }
    c->ack_same = same;
    c->window = pipe_window_for(&c->delay, percent);
    if (c->window < 1) {
        return fail("窗口算不出来");
    }
    c->bits = bits_of(kind, c->window);
    return 0;
}

static void print_case(const NeedCase *c, int percent, int kind, const char *title) {
    if (title != NULL) {
        printf("—— %s ——\n", title);
    }
    print_timing(&c->delay, c->ack_same);
    print_window_need(&c->delay, percent, c->window);
    print_bits(kind, c->window);
}

static int read_need(Line *a, int *kind, long long *lo, long long *hi, int *has_hi, long long *bps,
                     long long *prop, long long *ack, int *same, int *percent) {
    const char *name;

    if (!pull(a, &name) || !parse_kind(name, kind)) {
        return fail("协议写 stop、gbn、sr 或 slide");
    }
    if (!want(a, "frame") || !amount(a, 0, lo)) {
        return fail("frame 后面写长度和单位");
    }
    *has_hi = 0;
    if (a->i < a->n && strcmp(a->w[a->i], "to") == 0) {
        a->i++;
        if (!amount(a, 0, hi)) {
            return fail("to 后面写另一个长度和单位");
        }
        *has_hi = 1;
    }
    if (!want(a, "bw") || !amount(a, 1, bps) || !want(a, "prop") || !amount(a, 2, prop)) {
        return fail("bw 和 prop 后面都要写数值和单位");
    }
    if (!parse_ack(a, ack, same) || !parse_util(a, percent)) {
        return fail("ack 写 same 或「长度 单位」；util 是 1 到 100，不写则按 100");
    }
    return 0;
}

static int cmd_need(Line *a) {
    NeedCase left;
    NeedCase right;
    NeedCase *pick;
    int kind = 0;
    int has_hi = 0;
    int same = 0;
    int percent = 100;
    long long lo = 0;
    long long hi = 0;
    long long bps = 0;
    long long prop = 0;
    long long ack = 0;
    char left_name[64];
    char right_name[64];

    if (read_need(a, &kind, &lo, &hi, &has_hi, &bps, &prop, &ack, &same, &percent)) {
        return 1;
    }
    if (build_case(lo, ack, same, bps, prop, percent, kind, &left)) {
        return 1;
    }
    printf("【序号位数 · %s】\n", kind_name(kind));
    if (!has_hi || lo == hi) {
        print_case(&left, percent, kind, NULL);
        pick = &left;
    } else {
        if (build_case(hi, ack, same, bps, prop, percent, kind, &right)) {
            return 1;
        }
        puts("帧长有一个范围。帧越短，Td 越小，确认回来之前能连发的帧越多。");
        puts("范围内每一种长度都要达到目标利用率，就按窗口更大的那一档取序号。");
        fmt_bits(left_name, sizeof left_name, left.delay.frame_bits);
        fmt_bits(right_name, sizeof right_name, right.delay.frame_bits);
        print_case(&left, percent, kind, left_name);
        print_case(&right, percent, kind, right_name);
        pick = left.window >= right.window ? &left : &right;
        printf("起决定作用的是 %s 这一档。\n",
               pick == &left ? left_name : right_name);
    }
    if (kind == 0 && pick->window > 1) {
        puts("结果：停等达不到");
        return 0;
    }
    printf("结果：发送窗口至少 %d，序号至少 %d 位\n", pick->window, pick->bits);
    return 0;
}

static void print_rate_result(const PipeDelay *d, const PipeRate *r) {
    char nice[64];
    char mbps[32];

    if (r->saturated || r->rate_exact) {
        fmt_short_rate(nice, sizeof nice, r->saturated ? d->bps : r->rate_bps);
        printf("结果：%s\n", nice);
        return;
    }
    if (r->rate_milli >= 500) {
        printf("结果：%s%d Mbps\n", r->rate_approx ? "约 " : "", r->rate_whole);
        return;
    }
    fmt_milli(mbps, sizeof mbps, r->rate_milli, "Mbps");
    printf("结果：%s\n", mbps);
}

static void print_open_rate(const PipeDelay *d, const PipeRate *r) {
    char mbps[32];
    long long sent = d->frame_bits * r->window;

    printf("%d < %d，窗口会先发完，后面信道空着，不能直接答带宽。\n", r->window, r->full_window);
    printf("一个周期里发出 %d × %lld bit = %lld bit。\n", r->window, d->frame_bits, sent);
    if (r->rate_exact) {
        fmt_short_rate(mbps, sizeof mbps, r->rate_bps);
        printf("平均速率 = %lld bit / 周期 = %s。\n", sent, mbps);
        return;
    }
    fmt_milli(mbps, sizeof mbps, r->rate_milli, "Mbps");
    printf("平均速率 = %lld bit / 周期 = %s。\n", sent, mbps);
    printf("它离 %d Mbps 不到 0.5 Mbps，记成约 %d Mbps。\n", r->rate_whole, r->rate_whole);
}

static int cmd_rate(Line *a) {
    const char *name;
    PipeDelay delay;
    PipeRate rate;
    char err[180];
    char util[32];
    int kind = 0;
    int window = 0;
    int same = 0;
    long long frame = 0;
    long long bps = 0;
    long long prop = 0;
    long long ack = 0;

    if (!pull(a, &name) || !parse_kind(name, &kind) || kind == 3) {
        return fail("rate 的协议写 stop、gbn 或 sr");
    }
    if (!want(a, "window") || !pull(a, &name) || !parse_int(name, &window) || window < 1) {
        return fail("window 后面写正整数");
    }
    if (kind == 0 && window != 1) {
        return fail("停等的窗口只能是 1");
    }
    if (!want(a, "frame") || !amount(a, 0, &frame) || !want(a, "bw") || !amount(a, 1, &bps)) {
        return fail("frame 和 bw 后面都要写数值和单位");
    }
    if (!want(a, "prop") || !amount(a, 2, &prop) || !parse_ack(a, &ack, &same) || a->i != a->n) {
        return fail("prop 后面写时延；ack 写 same 或「长度 单位」");
    }
    if (!pipe_delays(frame, ack, same, bps, prop, &delay, err, (int)sizeof err)) {
        return fail(err);
    }
    if (!pipe_rate(&delay, window, &rate, err, (int)sizeof err)) {
        return fail(err);
    }
    printf("【平均数据传输速率 · %s】\n", kind_name(kind));
    print_timing(&delay, same);
    printf("发送窗口 W = %d\n", window);
    printf("把信道用满需要的窗口 = ceil(周期 / Td) = %d\n", rate.full_window);
    if (rate.saturated) {
        printf("%d >= %d，确认回来时窗口还没发完，利用率封顶为 100%%。\n", window, rate.full_window);
        puts("平均速率等于带宽。");
    } else {
        print_open_rate(&delay, &rate);
    }
    fmt_milli(util, sizeof util, rate.util_milli, "%");
    printf("利用率 U = W × Td / 周期 = %s\n", util);
    print_rate_result(&delay, &rate);
    return 0;
}

static int print_sr_recv(int bits, int ws, int space) {
    int recv = limit_max_sr_recv(bits, ws);

    if (recv < 1) {
        return fail("SR 的发送窗口要在 1 和 N-1 之间，好给接收窗口留位置");
    }
    printf("已给定发送窗口 Ws = %d。\n", ws);
    puts("SR 同时要求 Wr <= Ws，以及 Ws + Wr <= N。");
    printf("Wr <= %d，并且 Wr <= %d - %d = %d。更紧的是 %d。\n", ws, space, ws, space - ws, recv);
    puts("再大，确认丢失后重传的旧帧会落进新的接收窗口，被当成新帧。");
    printf("结果：接收窗口最大 %d\n", recv);
    return 0;
}

static int print_fixed_limit(int kind, int ws, int space) {
    if (kind == 0 && ws != -1) {
        return fail("停等不用写 send，窗口就是 1");
    }
    if (kind == 0) {
        puts("停等的发送窗口和接收窗口都是 1。多出来的序号位数不会把窗口放大。");
        puts("结果：发送窗口 1，接收窗口 1");
        return 0;
    }
    if (kind == 1 && ws != -1) {
        return fail("GBN 的接收窗口固定是 1，不用写 send");
    }
    if (kind == 1) {
        printf("接收窗口固定为 1。发送窗口最大 N - 1 = %d。\n", space - 1);
        puts("空出一个序号，确认全部丢失时才分得清重传的旧帧和新的一轮。");
        printf("结果：发送窗口最大 %d，接收窗口 1\n", space - 1);
        return 0;
    }
    return fail("协议无法识别");
}

static int cmd_limit(Line *a) {
    const char *name;
    int kind = 0;
    int bits = 0;
    int ws = -1;
    int space;
    int recv;

    if (!pull(a, &name) || !parse_kind(name, &kind) || kind == 3) {
        return fail("limit 的协议写 stop、gbn 或 sr");
    }
    if (!want(a, "bits") || !pull(a, &name) || !parse_int(name, &bits)) {
        return fail("bits 后面写序号位数");
    }
    if (a->i < a->n) {
        if (!want(a, "send") || !pull(a, &name) || !parse_int(name, &ws) || ws < 1 || a->i != a->n) {
            return fail("send 后面写发送窗口，并且命令到此结束");
        }
    }
    space = limit_space(bits);
    if (space == 0) {
        return fail("序号位数要在 1 到 16 之间");
    }
    printf("【窗口上限 · %s】\n", kind_name(kind));
    printf("序号 n = %d，序号空间 N = 2^%d = %d，序号 0～%d。\n", bits, bits, space, space - 1);
    if (kind == 0 || kind == 1) {
        return print_fixed_limit(kind, ws, space);
    }
    if (ws < 0) {
        recv = limit_max_sr_equal(bits);
        printf("收发窗口相等时，每个最大 N/2 = %d，并且 %d + %d = %d，没有超出序号空间。\n", recv,
               recv, recv, recv * 2);
        puts("再大一号，新窗口就会和上一轮的序号重叠。");
        printf("结果：发送窗口最大 %d，接收窗口最大 %d\n", recv, recv);
        return 0;
    }
    return print_sr_recv(bits, ws, space);
}

static int cmd_compare(Line *a) {
    const char *s;
    int bits = 0;
    int space;
    int gbn;
    int sr;

    if (!pull(a, &s) || !parse_int(s, &bits) || a->i != a->n) {
        return fail("compare 后面只写序号位数");
    }
    space = limit_space(bits);
    if (space == 0) {
        return fail("序号位数要在 1 到 16 之间");
    }
    gbn = limit_max_gbn(bits);
    sr = limit_max_sr_equal(bits);
    puts("【三种协议的最大利用率】");
    printf("序号 %d 位，序号空间 %d。\n", bits, space);
    puts("同一条信道、同样的帧长，忽略确认帧时，U = min(1, W × Td / (Td + 2×传播))。");
    puts("没封顶时 U 随 W 增大。各协议利用率的上限，就看它允许的最大窗口。");
    puts("停等 W1 = 1");
    printf("GBN W2 = 2^%d - 1 = %d\n", bits, gbn);
    printf("SR 收发窗口相等，W3 = 2^(%d-1) = %d\n", bits, sr);
    puts("因此 U1 <= U3 <= U2。U1、U2、U3 分别是停等、GBN、SR。");
    if (gbn == sr) {
        puts("这里 GBN 和 SR 的最大窗口一样，U2 和 U3 取等号。");
    }
    puts("结果：U1 <= U3 <= U2");
    return 0;
}

int pipe_command(const char *line) {
    Line a;

    if (split_line(line, &a) != 0) {
        return 1;
    }
    if (a.n < 1) {
        return fail("空命令");
    }
    if (strcmp(a.w[0], "need") == 0) {
        a.i = 1;
        return cmd_need(&a);
    }
    if (strcmp(a.w[0], "rate") == 0) {
        a.i = 1;
        return cmd_rate(&a);
    }
    if (strcmp(a.w[0], "limit") == 0) {
        a.i = 1;
        return cmd_limit(&a);
    }
    if (strcmp(a.w[0], "compare") == 0) {
        a.i = 1;
        return cmd_compare(&a);
    }
    return fail("不认识的命令。可用 need / rate / limit / compare");
}
