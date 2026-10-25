#ifndef DELAY_H
#define DELAY_H

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    long long bits;
    long long bps;
    long long meters;
    long long mps;
    long long proc_ns;
    long long queue_ns;
    long long tx_ns;
    long long prop_ns;
    long long total_ns;
    long long rtt_ns;
    long long bdp_bits;
} LinkOut;

/* 一条链路上一个分组的四种时延。RTT 这里只取 2×传播时延 */
bool link_compute(long long bits, long long bps, long long meters, long long mps,
                  long long proc_ns, long long queue_ns, LinkOut *out);
void link_report(FILE *out, const LinkOut *r);

typedef struct {
    long long data_bits;
    long long ack_bits;
    long long bps;
    long long prop_ns;
    long long td_ns;
    long long ta_ns;
    long long rtt_ns;
    long long cycle_ns;
} UtilOut;

/* 停等：U = Td / (Td + Ta + 2×单向传播)。窗口大于 1 的利用率不在本关 */
bool util_compute(long long data_bits, long long ack_bits, long long bps,
                  long long prop_ns, UtilOut *out);
void util_report(FILE *out, const UtilOut *u);

bool bottleneck_bps(const long long *bps, int n, long long *out);
void bottleneck_report(FILE *out, const long long *bps, int n, long long best);

bool goodput_bps(long long payload_bits, long long header_bits, long long bps,
                 long long *out);
void goodput_report(FILE *out, long long payload, long long header, long long bps,
                    long long result);

#endif
