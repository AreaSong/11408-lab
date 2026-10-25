#ifndef MIP_H
#define MIP_H

#define MIP_PAYLOAD_MAX 32

/* 用第 01 关的 packet_build 生成内层 IP 数据报，不带以太网头。 */
int mip_datagram(const unsigned char src[4], const unsigned char dst[4],
                 const unsigned char *payload, int payload_len, unsigned char *ip, int cap,
                 int *ip_len, char *err, int err_cap);

/* 外层协议号 4。目的地址是转交地址，数据部分是整份内层数据报。 */
int mip_wrap(const unsigned char *inner, int inner_len, const unsigned char src[4],
             const unsigned char dst[4], unsigned char ttl, unsigned char *outer, int cap,
             int *outer_len, char *err, int err_cap);

/* 沿途路由器只改外层 TTL 和外层首部校验和。 */
int mip_hop(unsigned char *outer, int len, char *err, int err_cap);

#endif
