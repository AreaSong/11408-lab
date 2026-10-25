#ifndef MCAST_H
#define MCAST_H

/* D 类才返回 1。MAC 是 01-00-5e 再接 IP 的低 23 位，第 25 位写成 0。 */
int mcast_map(const unsigned char ip[4], unsigned char mac[6]);

/* hi 翻开第 2 字节最高位，top 改第 1 字节低 4 位，other 改已经映射进去的一位。 */
void mcast_kin(const unsigned char ip[4], unsigned char hi[4], unsigned char top[4],
               unsigned char other[4]);

/* 丢掉的 5 位任意取值，2^5。 */
int mcast_same_count(void);

#endif
