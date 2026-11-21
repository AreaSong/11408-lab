#ifndef DEV_BUS_H
#define DEV_BUS_H

/*
 * 总线带宽只算数据阶段：工作频率 × 一次的字节数 × 双工倍数 × 通道数。
 * 工作频率 = 时钟频率 × 每个时钟的传送次数 / 时钟数。地址和等待不进这个数。
 * 带宽里的 M、G 是 10^6、10^9。
 */
typedef struct {
    double bps;
    char brief[32];
} DevBw;

typedef struct {
    double seconds;
    double avg_bps;
    double peak_bps;
    char time_brief[32];
    char avg_brief[32];
    char peak_brief[32];
} DevTxn;

int dev_bw(double hz, double clocks, double beats, double bits, double duplex, double channels,
           DevBw *out);

/* each 非 0：每次数据都重送地址，一共 times 次。突发时地址只送一次，times 不用。 */
int dev_txn(int each, double hz, double times, double addr, double wait, double data, double prep,
            double bits, double payload, DevTxn *out);

#endif
