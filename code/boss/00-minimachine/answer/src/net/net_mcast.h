#ifndef NET_MCAST_H
#define NET_MCAST_H

/* 低 23 位进 MAC，第 25 位写成 0。丢掉的是 5 位，所以 32 个 IP 共用一个 MAC。 */
int net_mcast_mac(const char *ip, char *mac, int cap, int *alias);

#endif
