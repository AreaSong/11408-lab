#ifndef NET_CONG_H
#define NET_CONG_H

/*
 * 窗口单位是 MSS。rwnd < 0 表示接收窗口不限制。
 * timeout 为 1 时先用旧 cwnd 算门限，再把 cwnd 设成 1。
 * 慢开始按本轮实际发出的段数加；到门限就停。拥塞避免每轮加 1，哪怕这轮没发满。
 */
int net_cong_run(int cwnd, int thresh, int rwnd, int buf, int timeout, int rounds, int *out_cwnd,
                 int *out_swnd, int *out_rwnd);
int net_cong_trace(int kb0, int rounds, int *thresh, int *out, int cap);
int net_cong_back(int kb);
int net_cong_span(int from, int to, int slow);
int net_cong_fast(int kb, int *thresh, int *cwnd);
int net_cong_2016(int *rwnd, int *cwnd, int *swnd, unsigned *seq, int *rounds, int *kbps_x100);
int net_cong_ftp(int ackno, int *mss);
int net_cong_line(const char *id, char *out, int cap);

#endif
