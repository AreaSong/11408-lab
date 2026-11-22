#ifndef CSMACA_H
#define CSMACA_H

/*
 * 考纲：CSMA/CA。无线上检测不到冲突，靠退避、帧间间隔和预约来避开。
 * SIFS 短于 DIFS，CTS 和 ACK 才能抢在新的 RTS 前面。
 * 隐藏站收到 CTS 时，NAV 从 CTS 结束起算：SIFS + 数据帧 + SIFS + ACK。
 * 2024-36 的 354 us 不含 DIFS、RTS，也不再加已经发完的 CTS。
 */

typedef struct {
    long long difs_ns;
    long long sifs_ns;
    long long slot_ns;
    int backoff;
    long long rts_ns;
    long long cts_ns;
    long long data_ns;
    long long ack_ns;
} CaSpec;

typedef struct {
    long long backoff_ns;
    long long nav_cts_ns;
    long long total_ns;
} CaTime;

int ca_nav(long long sifs_ns, long long data_ns, long long ack_ns, long long *nav_ns);
int ca_time(const CaSpec *in, CaTime *out);

#endif
