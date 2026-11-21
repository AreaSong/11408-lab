#ifndef NET_DELAY_H
#define NET_DELAY_H

/* 时延口径同 net/00：k/M/G 是 10 的幂，RTT 只含往返传播。除不尽就返回 -1。 */
typedef struct {
    long long tx_ns;
    long long prop_ns;
    long long total_ns;
    long long rtt_ns;
    long long bdp_bit;
} NetDelay;

int net_link(long long bits, long long bps, long long meters, long long mps, long long proc_ns,
             long long queue_ns, NetDelay *out);
int net_util_pct(long long data_bits, long long ack_bits, long long bps, long long prop_ns, int *pct);
int net_goodput(long long data_bits, long long header_bits, long long bps, long long *out);
/* 有效数据率的分子是净荷，分母含整帧和确认帧的发送时延。2010-47 用这个。 */
int net_eff(long long payload, long long frame, long long ack, long long bps, long long prop_ns,
            long long *out);

#endif
