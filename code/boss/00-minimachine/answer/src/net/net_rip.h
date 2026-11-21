#ifndef NET_RIP_H
#define NET_RIP_H

/* 直连记 1。16 是不可达，没有 17。下一跳就是通告者时，好坏都收。 */
int net_rip_rules(char row[5][32]);
int net_rip_2016(int *dist, char *via, int cap);
int net_rip_slow(int split, int *rounds);
int net_rip_when(int origin, int *seconds);
int net_rip_via(int origin, int node, int *dist, int *via);
int net_rip_line(const char *id, char *out, int cap);

#endif
