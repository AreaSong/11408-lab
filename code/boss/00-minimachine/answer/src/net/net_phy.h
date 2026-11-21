#ifndef NET_PHY_H
#define NET_PHY_H

int net_nyquist(long long hz, int states, long long *bps);
int net_shannon(long long hz, int db, int percent, long long *bps, int *approx);
int net_qam(long long hz, long long bps, char *name, int cap);
int net_min_states(int db);
int net_manch(const char *bits, char *out, int cap);
int net_manch_baud(long long bps, long long *baud);

#endif
