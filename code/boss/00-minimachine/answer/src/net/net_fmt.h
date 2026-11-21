#ifndef NET_FMT_H
#define NET_FMT_H

/* 把纳秒和比特率收成考试里常见的写法。不能整除时才加「约」。 */
void net_fmt_ns(char *dst, int cap, long long ns);
void net_fmt_bps(char *dst, int cap, long long bps);
void net_fmt_pct(char *dst, int cap, long long num, long long den);

#endif
