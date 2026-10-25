#include "delay.h"

#include "qty.h"

#include <limits.h>
#include <stdio.h>

bool link_compute(long long bits, long long bps, long long meters, long long mps,
                  long long proc_ns, long long queue_ns, LinkOut *out) {
    long long sum;

    if (out == NULL || bits <= 0 || bps <= 0 || meters < 0 || mps <= 0) {
        return false;
    }
    if (proc_ns < 0 || queue_ns < 0) {
        return false;
    }
    if (!qty_div_ns(bits, bps, &out->tx_ns)) {
        return false;
    }
    if (!qty_div_ns(meters, mps, &out->prop_ns)) {
        return false;
    }
    /* 时延带宽积 = 传播时延 × 带宽，单位比特 */
    if (!qty_mul_div(out->prop_ns, bps, 1000000000LL, &out->bdp_bits)) {
        return false;
    }
    sum = out->tx_ns;
    if (sum > LLONG_MAX - out->prop_ns) {
        return false;
    }
    sum += out->prop_ns;
    if (sum > LLONG_MAX - proc_ns || sum + proc_ns > LLONG_MAX - queue_ns) {
        return false;
    }
    out->bits = bits;
    out->bps = bps;
    out->meters = meters;
    out->mps = mps;
    out->proc_ns = proc_ns;
    out->queue_ns = queue_ns;
    out->total_ns = sum + proc_ns + queue_ns;
    if (out->prop_ns > LLONG_MAX / 2) {
        return false;
    }
    out->rtt_ns = out->prop_ns * 2;
    return true;
}

void link_report(FILE *out, const LinkOut *r) {
    char bits[128], rate[96], dist[64], tx[80], prop[80], proc[80], queue[80];
    char total[80], rtt[80], bdp[128], total1[64], rtt1[64];

    fmt_bits(bits, sizeof bits, r->bits);
    fmt_bps(rate, sizeof rate, r->bps);
    fmt_meters(dist, sizeof dist, r->meters);
    fmt_time(tx, sizeof tx, r->tx_ns);
    fmt_time(prop, sizeof prop, r->prop_ns);
    fmt_time(proc, sizeof proc, r->proc_ns);
    fmt_time(queue, sizeof queue, r->queue_ns);
    fmt_time(total, sizeof total, r->total_ns);
    fmt_time(rtt, sizeof rtt, r->rtt_ns);
    fmt_span(bdp, sizeof bdp, r->bdp_bits);
    fmt_time_one(total1, sizeof total1, r->total_ns);
    fmt_time_one(rtt1, sizeof rtt1, r->rtt_ns);

    fputs("【时延构成】\n", out);
    qty_print_scale_note(out);
    fprintf(out, "数据长度 = %s\n", bits);
    fprintf(out, "带宽 R = %s\n", rate);
    fprintf(out, "距离 = %s\n", dist);
    fprintf(out, "传播速率 = %lld m/s\n", r->mps);
    fprintf(out, "发送时延（传输时延）= 数据长度 / 带宽 = %lld bit / %lld bps = %s\n",
            r->bits, r->bps, tx);
    fprintf(out, "传播时延 = 距离 / 传播速率 = %lld m / %lld m/s = %s\n",
            r->meters, r->mps, prop);
    fprintf(out, "处理时延 = %s\n", proc);
    fprintf(out, "排队时延 = %s\n", queue);
    fprintf(out, "总时延 = 发送 + 传播 + 处理 + 排队 = %s\n", total);
    fprintf(out, "往返时间 RTT = 2 × 传播时延 = %s\n", rtt);
    fputs("这里的 RTT 只含往返传播。题目若已把发送时延算进 RTT，不能再加一次。\n", out);
    fprintf(out, "时延带宽积 = 传播时延 × 带宽 = %lld ns × %lld bps / 10^9 = %s\n",
            r->prop_ns, r->bps, bdp);
    fputs("时延带宽积的单位是比特，等于这段单向链路同时能容纳的比特数。\n", out);
    fprintf(out, "结果：总时延 %s，RTT %s，时延带宽积 %s\n", total1, rtt1, bdp);
}

static void fmt_percent(char *buf, size_t n, long long num, long long den) {
    if (den > 0 && num <= LLONG_MAX / 100 && (num * 100) % den == 0) {
        snprintf(buf, n, "%lld%%", (num * 100) / den);
        return;
    }
    snprintf(buf, n, "%lld/%lld", num, den);
}

bool util_compute(long long data_bits, long long ack_bits, long long bps,
                  long long prop_ns, UtilOut *out) {
    if (out == NULL || data_bits <= 0 || ack_bits < 0 || bps <= 0 || prop_ns < 0) {
        return false;
    }
    if (!qty_div_ns(data_bits, bps, &out->td_ns)) {
        return false;
    }
    if (!qty_div_ns(ack_bits, bps, &out->ta_ns)) {
        return false;
    }
    if (prop_ns > LLONG_MAX / 2) {
        return false;
    }
    out->rtt_ns = prop_ns * 2;
    if (out->td_ns > LLONG_MAX - out->ta_ns) {
        return false;
    }
    if (out->td_ns + out->ta_ns > LLONG_MAX - out->rtt_ns) {
        return false;
    }
    out->data_bits = data_bits;
    out->ack_bits = ack_bits;
    out->bps = bps;
    out->prop_ns = prop_ns;
    out->cycle_ns = out->td_ns + out->ta_ns + out->rtt_ns;
    return out->cycle_ns > 0;
}

void util_report(FILE *out, const UtilOut *u) {
    char data[128], ack[128], rate[96], prop[80], td[80], ta[80], rtt[80], cycle[80];
    char td1[64], ta1[64], rtt1[64], td_ms[64], cycle_ms[64], pct[64];

    fmt_bits(data, sizeof data, u->data_bits);
    fmt_bits(ack, sizeof ack, u->ack_bits);
    fmt_bps(rate, sizeof rate, u->bps);
    fmt_time(prop, sizeof prop, u->prop_ns);
    fmt_time(td, sizeof td, u->td_ns);
    fmt_time(ta, sizeof ta, u->ta_ns);
    fmt_time(rtt, sizeof rtt, u->rtt_ns);
    fmt_time(cycle, sizeof cycle, u->cycle_ns);
    fmt_time_one(td1, sizeof td1, u->td_ns);
    fmt_time_one(ta1, sizeof ta1, u->ta_ns);
    fmt_time_one(rtt1, sizeof rtt1, u->rtt_ns);
    fmt_time_ms(td_ms, sizeof td_ms, u->td_ns);
    fmt_time_ms(cycle_ms, sizeof cycle_ms, u->cycle_ns);
    fmt_percent(pct, sizeof pct, u->td_ns, u->cycle_ns);

    fputs("【信道利用率 · 停等】\n", out);
    qty_print_scale_note(out);
    fprintf(out, "数据帧 = %s\n", data);
    fprintf(out, "确认帧 = %s\n", ack);
    fprintf(out, "带宽 R = %s\n", rate);
    fprintf(out, "单向传播时延 = %s\n", prop);
    fprintf(out, "数据帧发送时延 Td = %lld bit / %lld bps = %s\n",
            u->data_bits, u->bps, td);
    fprintf(out, "确认帧发送时延 Ta = %lld bit / %lld bps = %s\n",
            u->ack_bits, u->bps, ta);
    fprintf(out, "往返传播时延 = 2 × 单向传播时延 = %s\n", rtt);
    fprintf(out, "停等周期 = Td + Ta + 往返传播 = %s + %s + %s = %s\n",
            td1, ta1, rtt1, cycle);
    fprintf(out, "信道利用率 U = Td / 周期 = %s / %s = %s\n", td_ms, cycle_ms, pct);
    fputs("本关的窗口是 1。发送窗口大于 1 时的利用率在可靠传输那一关算。\n", out);
    fprintf(out, "结果：%s\n", pct);
}

bool bottleneck_bps(const long long *bps, int n, long long *out) {
    long long best;
    int i;

    if (bps == NULL || out == NULL || n < 1) {
        return false;
    }
    best = bps[0];
    if (best <= 0) {
        return false;
    }
    for (i = 1; i < n; i++) {
        if (bps[i] <= 0) {
            return false;
        }
        if (bps[i] < best) {
            best = bps[i];
        }
    }
    *out = best;
    return true;
}

void bottleneck_report(FILE *out, const long long *bps, int n, long long best) {
    char rate[96], answer[96];
    int i;

    fputs("【吞吐量】\n", out);
    qty_print_scale_note(out);
    for (i = 0; i < n; i++) {
        fmt_bps(rate, sizeof rate, bps[i]);
        fprintf(out, "第 %d 段 = %s\n", i + 1, rate);
    }
    fmt_bps(answer, sizeof answer, best);
    fputs("吞吐量等于最慢一段的速率。\n", out);
    fprintf(out, "结果：%s\n", answer);
}

bool goodput_bps(long long payload_bits, long long header_bits, long long bps,
                 long long *out) {
    long long total;

    if (out == NULL || payload_bits <= 0 || header_bits < 0 || bps <= 0) {
        return false;
    }
    if (payload_bits > LLONG_MAX - header_bits) {
        return false;
    }
    total = payload_bits + header_bits;
    return qty_mul_div(payload_bits, bps, total, out);
}

void goodput_report(FILE *out, long long payload, long long header, long long bps,
                    long long result) {
    char data[128], head[128], packet[128], rate[96], answer[96];

    fmt_bits(data, sizeof data, payload);
    fmt_bits(head, sizeof head, header);
    fmt_bits(packet, sizeof packet, payload + header);
    fmt_bps(rate, sizeof rate, bps);
    fmt_bps(answer, sizeof answer, result);
    fputs("【有效吞吐】\n", out);
    qty_print_scale_note(out);
    fprintf(out, "数据 = %s\n", data);
    fprintf(out, "首部 = %s\n", head);
    fprintf(out, "分组长度 = %s\n", packet);
    fprintf(out, "带宽 R = %s\n", rate);
    fprintf(out, "有效吞吐 = 数据 / 分组长度 × 带宽 = %lld / %lld × %lld bps = %s\n",
            payload, payload + header, bps, answer);
    fprintf(out, "结果：%s\n", answer);
}
