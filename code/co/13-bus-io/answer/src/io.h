#ifndef IO_H
#define IO_H

#include <stdio.h>

/* 占比 = 每秒次数 × 每次时钟周期 / 主频。ratio 为 0.025 时是 2.5%。 */
double share_ratio(double hz, double times, double cycles);
/* 传输率除以每次字节数。查询间隔是它的倒数。 */
double times_from_rate(double bytes_per_s, double bytes);
/* 准备时间不短于一次服务，缓冲才不会被下一笔盖掉。 */
int keeps_up(double prepare_s, double service_s);

/*
 * 任务时间含设备准备，但启动设备之后的指令与下一次准备重叠。
 * CPU 时间不含准备，服务程序的全部指令都算。
 * extra_last 是最后一个字符那几条没被重叠掉的时钟周期，标准解答可以不加。
 */
typedef struct {
    double total_cycles;
    double cpu_cycles;
    double extra_last;
} TaskClocks;

TaskClocks task_clocks(double hz, double gap_s, double cpi, double resp, double insns,
                       double launch, double count);
/* 字节数先换成位，再除以缓冲区位数。除不尽返回 -1。 */
long long dma_requests(long long nbytes, long long buf_bits);

int io_command(const char *cmd);

#endif
