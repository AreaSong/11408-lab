#ifndef NET_SDN_H
#define NET_SDN_H

/* 流表是控制器算好再下发的。数据平面转发时不再跑 Dijkstra。 */
int net_sdn_hop(int broken, int *hop);
int net_sdn_line(const char *id, char *out, int cap);

#endif
