#include "cong_show.h"

#include "line.h"
#include "units.h"

#include <stdio.h>

static void put_bytes(long n, char *out, size_t cap) {
    fmt_bytes(n, out, cap);
}

static void rwnd_text(long rwnd, int limited, char *out, size_t cap) {
    if (!limited || rwnd < 0) {
        snprintf(out, cap, "不限");
    } else {
        fmt_bytes(rwnd, out, cap);
    }
}

static void print_snap(const RoundSnap *s) {
    char sent[64], c0[64], c1[64], th[64], rw[64], sw[64], used[64];
    const char *phase = s->slow ? "慢开始" : "拥塞避免";

    put_bytes(s->sent, sent, sizeof sent);
    put_bytes(s->cwnd0, c0, sizeof c0);
    put_bytes(s->cwnd1, c1, sizeof c1);
    put_bytes(s->ssthresh, th, sizeof th);
    put_bytes(s->swnd1, sw, sizeof sw);
    rwnd_text(s->rwnd1, s->limited, rw, sizeof rw);
    printf("第 %d 个 RTT：%s，发出 %s，拥塞窗口 %s → %s，门限 %s",
           s->index, phase, sent, c0, c1, th);
    if (s->use_buf) {
        put_bytes(s->buffered1, used, sizeof used);
        printf("，缓存已用 %s，通告接收窗口 %s，发送窗口 %s\n", used, rw, sw);
    } else if (!s->limited) {
        printf("，接收窗口不限，发送窗口 %s\n", sw);
    } else {
        printf("，接收窗口 %s，发送窗口 %s\n", rw, sw);
    }
}

static void print_caps(const RoundSnap *snaps, int n) {
    int i;

    for (i = 0; i < n; i++) {
        char up[64], stop[64];

        if (!snaps[i].capped) {
            continue;
        }
        put_bytes(snaps[i].cwnd0 + snaps[i].sent, up, sizeof up);
        put_bytes(snaps[i].cwnd1, stop, sizeof stop);
        printf("第 %d 个 RTT 慢开始按发出的数据可以涨到 %s，门限把它停在 %s。\n",
               snaps[i].index, up, stop);
    }
}

static void print_min(const RoundSnap *s) {
    char cw[64], rw[64], sw[64];

    put_bytes(s->cwnd1, cw, sizeof cw);
    put_bytes(s->swnd1, sw, sizeof sw);
    if (!s->limited) {
        printf("接收窗口不限制发送，发送窗口等于拥塞窗口 %s。\n", cw);
        return;
    }
    rwnd_text(s->rwnd1, 1, rw, sizeof rw);
    printf("发送窗口 = min(拥塞窗口 %s, 接收窗口 %s) = %s。\n", cw, rw, sw);
}

static void print_block(const char *title, const RoundSnap *snaps, int n, int with_result) {
    int i;
    char cw[64], sw[64];

    output_begin();
    printf("%s\n", title);
    for (i = 0; i < n; i++) {
        print_snap(&snaps[i]);
    }
    print_caps(snaps, n);
    if (n > 0) {
        print_min(&snaps[n - 1]);
    }
    if (with_result && n > 0) {
        put_bytes(snaps[n - 1].cwnd1, cw, sizeof cw);
        put_bytes(snaps[n - 1].swnd1, sw, sizeof sw);
        printf("结果：拥塞窗口 %s，发送窗口 %s\n", cw, sw);
    }
}

void show_init(const Cong *c) {
    char cw[64], th[64], rw[64], sw[64];
    long rwnd = cong_rwnd(c);

    output_begin();
    put_bytes(c->cwnd, cw, sizeof cw);
    put_bytes(c->ssthresh, th, sizeof th);
    put_bytes(cong_swnd(c), sw, sizeof sw);
    rwnd_text(rwnd, rwnd >= 0, rw, sizeof rw);
    printf("【初始】\n");
    if (rwnd < 0) {
        printf("拥塞窗口 %s，慢开始门限 %s，接收窗口不限。\n", cw, th);
    } else {
        printf("拥塞窗口 %s，慢开始门限 %s，接收窗口 %s。\n", cw, th, rw);
    }
    if (rwnd < 0) {
        printf("接收窗口不限制发送，发送窗口等于拥塞窗口 %s。\n", sw);
    } else {
        printf("发送窗口 = min(拥塞窗口, 接收窗口) = %s。\n", sw);
    }
    printf("结果：拥塞窗口 %s，发送窗口 %s\n", cw, sw);
}

void show_timeout(const Cong *c, long old_cwnd) {
    long half_n = (old_cwnd / c->mss) / 2;
    char old[64], raw[64], th[64], cw[64], sw[64];

    output_begin();
    put_bytes(old_cwnd, old, sizeof old);
    put_bytes(half_n * c->mss, raw, sizeof raw);
    put_bytes(c->ssthresh, th, sizeof th);
    put_bytes(c->cwnd, cw, sizeof cw);
    put_bytes(cong_swnd(c), sw, sizeof sw);
    printf("【超时】\n");
    printf("超时前拥塞窗口 %s。\n", old);
    printf("慢开始门限 = %s / 2 = %s。\n", old, raw);
    if (half_n < 2) {
        printf("不足 2 个 MSS，门限抬到 %s。\n", th);
    } else {
        printf("下限是 2 个 MSS，这里取 %s。\n", th);
    }
    printf("拥塞窗口改为 1 个 MSS，也就是 %s。接下来从慢开始长。\n", cw);
    printf("发送窗口 = %s。\n", sw);
    printf("结果：门限 %s，拥塞窗口 %s\n", th, cw);
}

void show_send(const Cong *c, long sent) {
    char a[64], b[64];

    output_begin();
    put_bytes(sent, a, sizeof a);
    put_bytes(c->in_flight, b, sizeof b);
    printf("【发送】\n");
    printf("发出 %s，在途变成 %s。\n", a, b);
    printf("结果：发出 %s\n", a);
}

void show_ack(const Cong *c, int n, long cwnd_before) {
    char before[64], after[64], th[64], rw[64], sw[64], buf[64], used[64];
    long rwnd = cong_rwnd(c);

    output_begin();
    put_bytes(cwnd_before, before, sizeof before);
    put_bytes(c->cwnd, after, sizeof after);
    put_bytes(c->ssthresh, th, sizeof th);
    put_bytes(cong_swnd(c), sw, sizeof sw);
    rwnd_text(rwnd, rwnd >= 0, rw, sizeof rw);
    printf("【确认 %d 个 MSS】\n", n);
    printf("对新数据的确认累计 %d 个。\n", c->new_acks);
    if (cwnd_before < c->ssthresh) {
        printf("慢开始：拥塞窗口 %s → %s，门限 %s。\n", before, after, th);
    } else {
        printf("拥塞避免：拥塞窗口 %s → %s，门限 %s。\n", before, after, th);
    }
    if (c->buf > 0) {
        put_bytes(c->buf, buf, sizeof buf);
        put_bytes(c->buffered, used, sizeof used);
        printf("接收缓存 %s，已用 %s，通告接收窗口 %s。\n", buf, used, rw);
    }
    printf("发送窗口 = min(拥塞窗口 %s, 接收窗口 %s) = %s。\n", after, rw, sw);
    printf("结果：接收窗口 %s，拥塞窗口 %s，发送窗口 %s\n", rw, after, sw);
}

void show_dup(const Cong *c, int fired, long old_cwnd) {
    char cw[64], old[64], th[64], plus3[64];

    output_begin();
    put_bytes(c->cwnd, cw, sizeof cw);
    printf("【第 %d 个重复确认】\n", c->dup_acks);
    if (!fired) {
        printf("确认号没有前进，拥塞窗口仍是 %s。\n", cw);
        printf("结果：重复确认 %d 个\n", c->dup_acks);
        return;
    }
    put_bytes(old_cwnd, old, sizeof old);
    put_bytes(c->ssthresh, th, sizeof th);
    put_bytes(c->ssthresh + 3 * c->mss, plus3, sizeof plus3);
    printf("连续 3 个重复确认，立刻重传丢失的段，不等超时计时器。\n");
    printf("慢开始门限 = %s / 2 = %s。\n", old, th);
    printf("快恢复把拥塞窗口设为门限 %s，之后每个 RTT 加 1 个 MSS。\n", cw);
    printf("有的实现会再加 3 个 MSS，变成 %s。王道这套题用 %s。\n", plus3, cw);
    printf("结果：门限 %s，拥塞窗口 %s\n", th, cw);
}

void show_rounds(const RoundSnap *snaps, int n) {
    char title[64];

    if (n == 1) {
        snprintf(title, sizeof title, "【1 个 RTT】");
    } else {
        snprintf(title, sizeof title, "【连续 %d 个 RTT】", n);
    }
    print_block(title, snaps, n, 1);
}

void show_until(const Cong *c, int kind, long target, const RoundSnap *snaps, int n) {
    char goal[64], one[64], total[64];
    char title[80];
    const char *which = kind == UNTIL_SWND ? "发送窗口" : "拥塞窗口";
    long all;

    put_bytes(target, goal, sizeof goal);
    snprintf(title, sizeof title, "【直到%s达到 %s】", which, goal);
    print_block(title, snaps, n, 0);
    if (c->rtt_us > 0) {
        all = (long)n * c->rtt_us;
        fmt_time_us(c->rtt_us, one, sizeof one);
        fmt_time_us(all, total, sizeof total);
        printf("%d × %s = %s。\n", n, one, total);
        printf("结果：%d RTT，%s\n", n, total);
        return;
    }
    printf("经过 %d 个 RTT。\n", n);
    printf("结果：%d RTT\n", n);
}

static void fmt_kbit(long bytes, long us, char *out, size_t cap) {
    long long bits = (long long)bytes * 8 * 1000000LL / us;
    long long whole = bits / 1000;
    long long frac = bits % 1000;

    if (frac == 0) {
        snprintf(out, cap, "%lld kbit/s", whole);
    } else {
        snprintf(out, cap, "%lld.%02lld kbit/s", whole, frac / 10);
    }
}

static void print_rate(const StallInfo *info) {
    long all = (long)info->rounds * info->rtt_us;
    char t_one[64], t_all[64], amount[64], per_text[64], rate[80], kbit[64];
    long long per_s = (long long)info->bytes * 1000000LL / all;
    long long dec_whole = per_s / 1000;
    long long dec_frac = (per_s % 1000) / 10;

    fmt_time_us(info->rtt_us, t_one, sizeof t_one);
    fmt_time_us(all, t_all, sizeof t_all);
    put_bytes(info->bytes, amount, sizeof amount);
    put_bytes((long)per_s, per_text, sizeof per_text);
    fmt_kbit(info->bytes, all, kbit, sizeof kbit);
    snprintf(rate, sizeof rate, "%s/s", per_text);
    printf("%d × %s = %s。\n", info->rounds, t_one, t_all);
    printf("共发送 %s。\n", amount);
    printf("平均速率 = %s / %s = %lld 字节/秒 = %s = %s。\n",
           amount, t_all, per_s, rate, kbit);
    printf("%lld 字节/秒 ÷ 1000 = %lld.%02lld kB/s，再乘 8 才是比特速率 %s。\n",
           per_s, dec_whole, dec_frac, kbit);
    printf("结果：序号 %ld，%d RTT，速率 %s（%s）\n",
           info->next_seq, info->rounds, rate, kbit);
}

void show_stall(const RoundSnap *snaps, const StallInfo *info) {
    long start = info->next_seq - info->bytes;

    print_block("【发到发送窗口为 0】", snaps, info->snap_n, 0);
    if (info->bytes % 1024 == 0) {
        printf("下一个待发送序号 = %ld + %ld × 1024 = %ld。\n",
               start, info->bytes / 1024, info->next_seq);
    } else {
        printf("下一个待发送序号 = %ld + %ld = %ld。\n",
               start, info->bytes, info->next_seq);
    }
    print_rate(info);
}
