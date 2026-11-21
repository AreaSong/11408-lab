#ifndef NET_TCP_H
#define NET_TCP_H

/* 第二次握手：SYN 和 ACK 都要是 1，确认号是客户初始序号加 1。服务器序号自己选。 */
int net_tcp_hs2(unsigned cisn, unsigned sisn, unsigned *seq, unsigned *ack);
unsigned net_tcp_hs3(unsigned sisn);
unsigned net_tcp_payload(unsigned syn_seq, unsigned fin_seq);
unsigned net_tcp_cumul(unsigned base, const int *len, const int *got, int n);
void net_tcp_reply(unsigned seq, unsigned ack, unsigned len, unsigned *oseq, unsigned *oack);
const char *net_tcp_active(int step);
int net_tcp_line(const char *id, char *out, int cap);

#endif
