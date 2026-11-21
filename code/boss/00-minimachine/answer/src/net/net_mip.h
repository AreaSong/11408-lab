#ifndef NET_MIP_H
#define NET_MIP_H

/* 外层目的是转交地址，内层目的仍是归属地址。回程不再进隧道。 */
int net_mip_tunnel(unsigned *cksum, unsigned *after, unsigned *colo, unsigned *colo_after);

#endif
