#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>

#define DHCP_STEPS 4

/*
 * 考纲：DHCP 首次获取 IP。客户还没有地址，源 IP 用 0.0.0.0。
 * 谢希仁把四步的目的 IP 都写成受限广播 255.255.255.255；
 * 客户端口 68，服务器端口 67，都走 UDP。收到 ACK 才进入已绑定。
 */
typedef struct {
    char name[16];
    char how[8];
    char src[32];
    char dst[32];
    int sport;
    int dport;
    char client[80];
    char server[80];
} DhcpStep;

/* server、offer 不能是 0.0.0.0、255.255.255.255，也不能相同。 */
int dhcp_initial(uint32_t server, uint32_t offer, DhcpStep step[DHCP_STEPS]);

/* 同一广播域才能听到发现报文。2022-47 的 E0 和 H4 同属一个 /25。 */
int dhcp_same_lan_hears_discover(int same_lan);

#endif
