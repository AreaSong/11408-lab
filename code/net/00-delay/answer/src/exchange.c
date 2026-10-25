#include "exchange.h"

#include "qty.h"

#include <limits.h>
#include <stdio.h>

static int fill_fixed(const ExchangeIn *in, long long setup, ExchangeOut *out) {
    long long hop;

    if (in == NULL || out == NULL) {
        return 0;
    }
    if (in->hops < 1 || in->hops > EXCHANGE_MAX_HOPS) {
        return 0;
    }
    if (in->bps <= 0 || in->prop_ns < 0 || setup < 0 || in->file_bits <= 0) {
        return 0;
    }
    if (in->packet_bits <= 0 || in->header_bits < 0) {
        return 0;
    }
    if (in->header_bits >= in->packet_bits) {
        return 0;
    }
    out->payload_bits = in->packet_bits - in->header_bits;
    out->uniform = (in->file_bits % out->payload_bits) == 0;
    if (!qty_div_ns(in->file_bits, in->bps, &out->file_tx_ns)) {
        return 0;
    }
    if (!qty_div_ns(in->packet_bits, in->bps, &out->tx_ns)) {
        return 0;
    }
    if (!qty_mul_div(in->prop_ns, in->hops, 1, &out->prop_sum_ns)) {
        return 0;
    }
    if (setup > LLONG_MAX - out->file_tx_ns) {
        return 0;
    }
    if (setup + out->file_tx_ns > LLONG_MAX - out->prop_sum_ns) {
        return 0;
    }
    out->circuit_ns = setup + out->file_tx_ns + out->prop_sum_ns;
    if (out->file_tx_ns > LLONG_MAX - in->prop_ns) {
        return 0;
    }
    hop = out->file_tx_ns + in->prop_ns;
    return qty_mul_div(hop, in->hops, 1, &out->message_ns);
}

/* 每一跳必须收完整个分组，并且该链路发完上一个分组，才能开始下一次发送 */
static int simulate(const ExchangeIn *in, long long *total, int *packets) {
    long long link_free[EXCHANGE_MAX_HOPS];
    long long sent = 0;
    long long last = 0;
    int count = 0;
    int hop;

    for (hop = 0; hop < in->hops; hop++) {
        link_free[hop] = 0;
    }
    while (sent < in->file_bits) {
        long long data = in->file_bits - sent;
        long long tx;
        long long ready = 0;

        if (data > in->packet_bits - in->header_bits) {
            data = in->packet_bits - in->header_bits;
        }
        if (!qty_div_ns(data + in->header_bits, in->bps, &tx)) {
            return 0;
        }
        for (hop = 0; hop < in->hops; hop++) {
            long long start = link_free[hop] > ready ? link_free[hop] : ready;
            if (start > LLONG_MAX - tx) {
                return 0;
            }
            link_free[hop] = start + tx;
            if (link_free[hop] > LLONG_MAX - in->prop_ns) {
                return 0;
            }
            ready = link_free[hop] + in->prop_ns;
        }
        last = ready;
        sent += data;
        count++;
        if (count > 1000000) {
            return 0;
        }
    }
    *total = last;
    *packets = count;
    return 1;
}

static int check_uniform(const ExchangeIn *in, ExchangeOut *out) {
    long long steps;
    long long tx_part;

    if (!out->uniform) {
        out->source_ns = 0;
        out->extra_ns = 0;
        return 1;
    }
    steps = (long long)out->packets + in->hops - 1;
    if (!qty_mul_div(out->tx_ns, out->packets, 1, &out->source_ns)) {
        return 0;
    }
    if (!qty_mul_div(out->tx_ns, (long long)in->hops - 1, 1, &out->extra_ns)) {
        return 0;
    }
    if (!qty_mul_div(out->tx_ns, steps, 1, &tx_part)) {
        return 0;
    }
    if (tx_part > LLONG_MAX - out->prop_sum_ns) {
        return 0;
    }
    /* 等长分组的闭式必须和逐跳模拟相同，否则流水线写错了 */
    return tx_part + out->prop_sum_ns == out->packet_ns;
}

bool exchange_compute(const ExchangeIn *in, long long setup_ns, ExchangeOut *out) {
    if (!fill_fixed(in, setup_ns, out)) {
        return false;
    }
    if (!simulate(in, &out->packet_ns, &out->packets)) {
        return false;
    }
    return check_uniform(in, out);
}

static void print_identity(FILE *out, const ExchangeIn *in, const ExchangeOut *r) {
    char rate[96], packet[128], header[128], payload[128], file[128];

    fmt_bps(rate, sizeof rate, in->bps);
    fmt_bits(packet, sizeof packet, in->packet_bits);
    fmt_bits(header, sizeof header, in->header_bits);
    fmt_bits(payload, sizeof payload, r->payload_bits);
    fmt_bits(file, sizeof file, in->file_bits);
    qty_print_scale_note(out);
    fprintf(out, "链路段数 k = %d 段，中间结点 = %d 个\n", in->hops, in->hops - 1);
    fputs("段数比中间结点多 1：两端主机之间的每一段链路都要算，主机自己不是中间结点。\n", out);
    fprintf(out, "带宽 R = %s\n", rate);
    fprintf(out, "分组长度 L = %s\n", packet);
    fprintf(out, "分组首部 = %s\n", header);
    fprintf(out, "分组数据 = %s\n", payload);
    fprintf(out, "文件 = %s\n", file);
    fprintf(out, "分组数 n = %lld / %lld = %d 个\n",
            in->file_bits, r->payload_bits, r->packets);
}

static void print_prop_line(FILE *out, const ExchangeIn *in, int from_bdp, long long bdp_bits) {
    char prop[80], bdp[128];

    fmt_time(prop, sizeof prop, in->prop_ns);
    if (!from_bdp) {
        fprintf(out, "每段传播时延 d = %s\n", prop);
        return;
    }
    fmt_span(bdp, sizeof bdp, bdp_bits);
    fprintf(out, "时延带宽积 = %s\n", bdp);
    fprintf(out, "传播时延 d = 时延带宽积 / 带宽 = %lld bit / %lld bps = %s\n",
            bdp_bits, in->bps, prop);
}

static void print_pipeline(FILE *out, const ExchangeIn *in, const ExchangeOut *r) {
    char t_full[80], t1[64], src1[64], extra1[64], prop1[64], total1[64];

    fmt_time(t_full, sizeof t_full, r->tx_ns);
    fmt_time_one(t1, sizeof t1, r->tx_ns);
    fmt_time_one(src1, sizeof src1, r->source_ns);
    fmt_time_one(extra1, sizeof extra1, r->extra_ns);
    fmt_time_one(prop1, sizeof prop1, r->prop_sum_ns);
    fmt_time_one(total1, sizeof total1, r->packet_ns);
    fprintf(out, "单段发送时延 t = L / R = %lld bit / %lld bps = %s\n",
            in->packet_bits, in->bps, t_full);
    if (!r->uniform) {
        fputs("最后一个分组更短，总时延按逐跳收完再转发来算。\n", out);
        return;
    }
    fprintf(out, "源端发完 = n × t = %d × %s = %s\n", r->packets, t1, src1);
    fputs("存储转发：一个分组没收完不能转发。流水线：后一个分组不用等前一个走完全程。\n", out);
    fprintf(out, "额外存储转发 = (k-1) × t = %d × %s = %s\n",
            in->hops - 1, t1, extra1);
    fprintf(out, "传播合计 = k × d = %s\n", prop1);
    fprintf(out, "分组交换 = n×t + (k-1)×t + k×d = %s + %s + %s = %s\n",
            src1, extra1, prop1, total1);
}

void exchange_report_pipe(FILE *out, const ExchangeIn *in, const ExchangeOut *r,
                          int from_bdp, long long bdp_bits) {
    char total1[64];

    fputs("【分组交换 · 存储转发】\n", out);
    print_identity(out, in, r);
    print_prop_line(out, in, from_bdp, bdp_bits);
    print_pipeline(out, in, r);
    fmt_time_one(total1, sizeof total1, r->packet_ns);
    fprintf(out, "结果：%s\n", total1);
}

static void print_condition(FILE *out, const ExchangeIn *in, long long setup,
                            const ExchangeOut *r) {
    char setup1[64], gate1[64];
    long long gate;

    if (!r->uniform || in->header_bits != 0) {
        fputs("分组带首部或末分组更短时，直接比较上面三个总时延。\n", out);
        return;
    }
    gate = r->extra_ns;
    fmt_time_one(setup1, sizeof setup1, setup);
    fmt_time_one(gate1, sizeof gate1, gate);
    fputs("首部为 0 且各组等长时，三种方式的传播都是 k×d，比较时抵消。\n", out);
    fprintf(out, "分组交换短于电路交换的条件：建立时延 > (k-1)×t，即 %s > %s。\n",
            setup1, gate1);
    if (setup > gate) {
        fputs("本题条件成立。\n", out);
    } else if (setup < gate) {
        fputs("本题条件不成立，电路交换更短。\n", out);
    } else {
        fputs("本题两边相等。\n", out);
    }
    if (r->packet_ns < r->message_ns) {
        fputs("分组交换仍短于报文交换：报文在每一段都发送整份文件。\n", out);
    }
}

void exchange_report_compare(FILE *out, const ExchangeIn *in, long long setup_ns,
                             const ExchangeOut *r) {
    char prop[80], setup_s[80], file_tx[80];
    char s1[64], f1[64], seg1[64], p1[64], c1[64], m1[64], k1[64];

    fmt_time(prop, sizeof prop, in->prop_ns);
    fmt_time(setup_s, sizeof setup_s, setup_ns);
    fmt_time(file_tx, sizeof file_tx, r->file_tx_ns);
    fmt_time_one(s1, sizeof s1, setup_ns);
    fmt_time_one(f1, sizeof f1, r->file_tx_ns);
    fmt_time_one(seg1, sizeof seg1, in->prop_ns);
    fmt_time_one(p1, sizeof p1, r->prop_sum_ns);
    fmt_time_one(c1, sizeof c1, r->circuit_ns);
    fmt_time_one(m1, sizeof m1, r->message_ns);
    fmt_time_one(k1, sizeof k1, r->packet_ns);

    fputs("【电路交换、报文交换、分组交换】\n", out);
    print_identity(out, in, r);
    fprintf(out, "每段传播时延 d = %s\n", prop);
    fprintf(out, "电路建立时延 = %s\n", setup_s);
    fprintf(out, "文件发送时延 = %lld bit / %lld bps = %s\n",
            in->file_bits, in->bps, file_tx);
    fprintf(out, "电路交换 = 建立 + 文件发送 + k×传播 = %s + %s + %s = %s\n",
            s1, f1, p1, c1);
    fputs("通路建好后比特直通，中间结点不存储整份数据，发送时延只在源端算一次。\n", out);
    fprintf(out, "报文交换 = k × (文件发送 + 传播) = %d × (%s + %s) = %s\n",
            in->hops, f1, seg1, m1);
    fputs("报文不拆开，每一段都要收完整个报文再转发。\n", out);
    fputs("分组交换按存储转发，分组之间可以流水线：\n", out);
    print_pipeline(out, in, r);
    print_condition(out, in, setup_ns, r);
    fprintf(out, "结果：电路交换 %s，报文交换 %s，分组交换 %s\n", c1, m1, k1);
}
