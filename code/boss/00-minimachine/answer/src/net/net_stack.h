#ifndef NET_STACK_H
#define NET_STACK_H

/* 一台路由器、两台主机，全在本进程里。用来把 ARP、转发、分片、NAT、DHCP 串起来。 */
int net_stack_line(char *out, int cap);

#endif
