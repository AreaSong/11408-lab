#ifndef UDP_SHOW_H
#define UDP_SHOW_H

#include "udp.h"

void udp_show_sum(unsigned short left, unsigned short right);
void udp_show_build(const UdpGram *g);
void udp_show_recv(const UdpGram *g);
void udp_show_header(const UdpGram *g);
void udp_show_demux(void);
void udp_show_eff(unsigned int app);

#endif
