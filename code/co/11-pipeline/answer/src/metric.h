#ifndef PIPE_METRIC_H
#define PIPE_METRIC_H

#include <stdio.h>

/* 理想流水线完成 n 条指令的时钟周期数：k + n - 1。 */
int metric_cycles(int k, int n);

/* 约分后的加速比、效率。加速比分子是 n×k，效率分子是 n，分母都是 k+n-1。 */
void metric_ratio(int k, int n, long long *s_num, long long *s_den,
                  long long *e_num, long long *e_den);

/* 时钟周期 = 最长功能段 + 段间锁存。单位由调用者自己记。 */
long long metric_period(const long long *stage, int nstage, long long latch);

/* 主频 mhz（MHz）、无阻塞时每秒完成的指令数。除不尽时返回 -1。 */
long long metric_tp(int k, int n, int mhz);

void metric_ideal(FILE *fp, int k, int n, long long dt, const char *unit);
void metric_freq(FILE *fp, int k, int n, int mhz);
void metric_clock(FILE *fp, const long long *stage, int nstage, long long latch,
                  const char *unit);

#endif
