#ifndef PIPE_H
#define PIPE_H

#include <stdbool.h>

/*
 * 一个确认周期 = Td + Ta + 2×单向传播。
 * 窗口为 W 时，没出错、也没封顶的利用率 U = W × Td / 周期。
 * W × Td >= 周期以后信道不再空闲，U 封顶为 1，速率等于带宽。
 * 时间用纳秒整数。除不尽的输入直接拒绝，不在这里四舍五入。
 */

typedef struct {
    long long frame_bits;
    long long ack_bits;
    long long bps;
    long long prop_ns;
    long long td_ns;
    long long ta_ns;
    long long cycle_ns;
} PipeDelay;

typedef struct {
    int window;
    int full_window;
    int saturated;
    int util_milli;  /* 百分比 ×1000，100000 表示 100% */
    int rate_exact;
    long long rate_bps;
    int rate_milli;  /* 未整除时的 Mbps×1000 */
    int rate_whole;
    int rate_approx;
} PipeRate;

/* ack_same 为真时，确认帧和数据帧一样长，ack_bits 被忽略。 */
bool pipe_delays(long long frame_bits, long long ack_bits, int ack_same, long long bps,
                 long long prop_ns, PipeDelay *d, char *err, int cap);

/* 使 U >= percent% 的最小窗口。percent 取 1～100。算不出时返回 0。 */
int pipe_window_for(const PipeDelay *d, int percent);

bool pipe_rate(const PipeDelay *d, int window, PipeRate *r, char *err, int cap);

#endif
