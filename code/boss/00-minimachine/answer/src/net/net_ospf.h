#ifndef NET_OSPF_H
#define NET_OSPF_H

/* pre[v] 是最短路树上的父亲。转发用的下一跳是离开源的第一台。 */
int net_ospf_2014(char hop[4][8], int cost[4]);
int net_ospf_line(const char *id, char *out, int cap);

#endif
