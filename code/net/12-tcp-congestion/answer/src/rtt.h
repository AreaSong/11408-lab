#ifndef RTT_H
#define RTT_H

/* 时间用微秒，乘除都是整数。α=1/8，β=1/4，和 RFC 6298 的系数一样。 */
typedef struct {
    long rtts_us;
    long rttd_us;
    long rto_us;
    int have;
    int ignore;
} RttEst;

typedef struct {
    int first;
    int ignored;
    long sample_us;
    long old_rtts_us;
    long old_rttd_us;
    long rtts_us;
    long rttd_us;
    long rto_us;
    long rfc_rto_us;
    long prev_rto_us;
} RttStep;

void rtt_clear(RttEst *e);
const char *rtt_set_rto(RttEst *e, long us);
const char *rtt_sample(RttEst *e, long sample_us, RttStep *step);
const char *rtt_rexmit(RttEst *e, long *rto_us);

#endif
