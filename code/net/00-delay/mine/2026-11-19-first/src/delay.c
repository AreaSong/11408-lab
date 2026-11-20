#include "delay.h"

long long send_delay_ns(long long bits, long long bps) {
    return bits * 100000000LL / bps;
}

long long prop_delay_ns(long long meters, long long speed_mps) {
    return meters * 100000000LL / speed_mps;
}

long long rtt_ns(long long prop_ns) {
    return 2 * prop_ns;
}

long long bdp_bits(long long prop_ns, long long bps) {
    return prop_ns * bps / 100000000LL;
}

double stop_wait_util(long long data_ns, long long ack_ns, long long prop_ns) {
    return (double) data_ns / (data_ns + ack_ns + 2 * prop_ns);
}
