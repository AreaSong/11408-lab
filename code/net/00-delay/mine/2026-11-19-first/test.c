#include "test.h"
#include "delay.h"


int main(void) {
    long long s = send_delay_ns(1000, 1000000);
    long long p = prop_delay_ns(400000, 200000000);

    CHECK_EQ(s, 10000000);
    CHECK_EQ(p, 2000000);

    CHECK_EQ(rtt_ns(p), 4000000);
    CHECK_EQ(bdp_bits(p,1000000), 2000);

    double u = stop_wait_util(4, 0, 3);
    CHECK(u>0.399&&u<0.401);

    return TEST_SUMMARY();
}
