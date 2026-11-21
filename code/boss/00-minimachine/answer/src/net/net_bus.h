#ifndef NET_BUS_H
#define NET_BUS_H

int net_bus_csma(int stations, int frames, int length, long long *useful, long long *total);
int net_bus_token(int stations, int frames, int length, int walk, long long *useful, long long *total);

#endif
