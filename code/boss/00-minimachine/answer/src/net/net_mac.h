#ifndef NET_MAC_H
#define NET_MAC_H

long long net_min_bits(long long bps, long long meters, long long mps);
long long net_delta_m(long long bits, long long bps, long long mps);
long long net_reach_m(long long frame_bits, long long bps, long long dev_ns, long long m_per_us);
long long net_one_way_ns(long long frame_bits, long long bps);
long long net_backoff_ns(int collisions, long long slot_ns);
int net_cdma_2014(char *out, int cap);
int net_cdma_decode(const int *chip, int nchip, const int *recv, int nrecv, char *out, int cap);

#endif
