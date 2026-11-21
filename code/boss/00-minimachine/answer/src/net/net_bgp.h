#ifndef NET_BGP_H
#define NET_BGP_H

/* 路径里看见自己的 AS 号就丢掉。无策略时先比 AS 个数，再比出口远近。 */
int net_bgp_drop(const int *path, int n, int self);
int net_bgp_kind(int as_a, int as_b, char *out, int cap);
int net_bgp_hop(int router, char *out, int cap);
int net_bgp_line(const char *id, char *out, int cap);
int net_nexam(const char *id, char *letter, int cap);

#endif
