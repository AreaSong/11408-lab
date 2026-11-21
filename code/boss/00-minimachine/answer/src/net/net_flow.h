#ifndef NET_FLOW_H
#define NET_FLOW_H

/* 发送窗口取拥塞窗口和通告窗口的较小值，还要扣掉已经发出还没确认的字节。 */
int net_flow_left(int cwnd, int rwnd, int inflight);

/* read_first 为 1：应用已经读空，但窗口通告丢了，探测字节被收下。 */
int net_flow_zero(int read_first, unsigned *seq, int *rwnd);

/* 样本单位是微秒。先更新 RTTs，偏差用新的 RTTs 和样本的差。重传过的样本不更新，RTO 翻倍。 */
int net_rto_pair(long *rto_us, long *karn_us);
int net_rto_line(char *out, int cap);
int net_flow_line(const char *id, char *out, int cap);

#endif
