#ifndef DEV_SHARE_H
#define DEV_SHARE_H

/*
 * CPU 用于 I/O 的时间占比 = 每秒次数 × 每次时钟周期 / 主频。
 * 查询和中断的次数 = 传输率 / 每次字节数。DMA 的次数 = 传输率 / 块字节数。
 * 传输率里的 M、G 是 10^6、10^9。准备时间不短于一次服务，缓冲才不会被盖掉。
 */
typedef struct {
    double times;
    double cycles;
    double ratio;
    int keeps;
    char pct[16];
    char gap[24];
} DevShare;

typedef struct {
    double bits;
    double per_s;
    char bits_text[16];
    char rate_text[24];
} DevFrame;

typedef struct {
    double total;
    double cpu;
    char total_text[32];
    char cpu_text[32];
} DevTask;

int dev_share_poll(double hz, double cycles, double times, DevShare *out);
int dev_share_query(double hz, double cpi, double insns, double bps, double bytes, DevShare *out);
int dev_share_intr(double hz, double cpi, double service, double extra, double bps, double bytes,
                   DevShare *out);
int dev_share_fit(double hz, double cycles, double bps, double bytes, DevShare *out);
int dev_share_dma(double hz, double cycles, double bps, double block, DevShare *out);

/* 字节数先换成位，再除以 DMA 缓冲区的位数。除不尽返回 -1。 */
long long dev_dma_requests(long long nbytes, long long buf_bits);

int dev_serial(double start, double data, double parity, double stop, double gap_s, DevFrame *out);

/*
 * 任务时间含设备准备。启动设备之后的指令和下一次准备重叠，不算进 CPU 时间。
 * 最后一个字符没被重叠掉的那几条，标准解答不加。
 */
int dev_task(double hz, double gap_s, double cpi, double resp, double insns, double launch,
             double count, DevTask *out);

#endif
