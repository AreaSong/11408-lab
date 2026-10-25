#ifndef EXCHANGE_H
#define EXCHANGE_H

#include <stdbool.h>
#include <stdio.h>

#define EXCHANGE_MAX_HOPS 32

typedef struct {
    int hops;
    long long bps;
    long long prop_ns;
    long long file_bits;
    long long packet_bits;
    long long header_bits;
} ExchangeIn;

typedef struct {
    long long payload_bits;
    int packets;
    int uniform;
    long long tx_ns;
    long long file_tx_ns;
    long long source_ns;
    long long extra_ns;
    long long prop_sum_ns;
    long long circuit_ns;
    long long message_ns;
    long long packet_ns;
} ExchangeOut;

/*
 * 三种交换用同一组参数。建立时延只加在电路交换上。
 * 报文交换整份存储转发；分组交换按逐跳模拟，等长分组时与 (n+k-1)×t + k×d 对账。
 */
bool exchange_compute(const ExchangeIn *in, long long setup_ns, ExchangeOut *out);

void exchange_report_pipe(FILE *out, const ExchangeIn *in, const ExchangeOut *r,
                          int from_bdp, long long bdp_bits);
void exchange_report_compare(FILE *out, const ExchangeIn *in, long long setup_ns,
                             const ExchangeOut *r);

#endif
