#ifndef NET_DHCP_H
#define NET_DHCP_H

#include <stdint.h>

/* 发现、提供、请求、确认。客户还没有地址时，发现报文是广播。 */
int net_dhcp_accept(int *state, const char *msg, uint32_t *addr);
int net_dhcp_line(char *out, int cap);

#endif
