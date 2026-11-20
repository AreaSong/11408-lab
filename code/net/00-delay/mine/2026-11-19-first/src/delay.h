#ifndef DELAY_H
#define DELAY_H

long long send_delay_ns(long long bits,long long bps);

long long prop_delay_ns(long long meters,long long speed_mps);

long long rtt_ns(long long prop_ns);

long long bdp_bits(long long prop_ns,long long bps);

double stop_wait_util(long long data_ns,long long ack_ns,long long prop_ns);

#endif
