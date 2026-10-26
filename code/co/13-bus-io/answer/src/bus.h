#ifndef BUS_H
#define BUS_H

#include <stdio.h>

/*
 * 总线带宽 = 工作频率 × 一次传送的字节数 × 双工倍数 × 通道数。
 * 工作频率 = 时钟频率 × beats / clocks。
 * 地址、裁决、等待都不进这个数，它是数据阶段的最大传输率。
 */
double bw_bps(double hz, double clocks, double beats, double bits, double duplex, double channels);

/* each 非 0：每一次数据都重送地址，times 次。突发时 times 忽略，地址只送一次。 */
double txn_seconds(int each, double hz, double times, double addr, double wait, double data,
                   double prep);

int bus_print_bw(FILE *out, double hz, double clocks, double beats, double bits, double duplex,
                 double channels);
int bus_print_txn(FILE *out, int each, double hz, double times, double addr, double wait,
                  double data, double prep, double bits, double payload);

/* 不是总线命令时返回 -1。 */
int bus_command(const char *cmd);

#endif
