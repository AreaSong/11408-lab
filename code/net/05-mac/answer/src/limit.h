#ifndef LIMIT_H
#define LIMIT_H

#include <stdbool.h>

/*
 * 最短帧的发送时间要盖住争用期 2τ。
 * 中间有集线器时，单程预算先等于争用期的一半，再扣设备时延，剩下的才是传播。
 * 2016-36 若先从整个争用期里扣一次设备时延再除以 2，会得到约 359 m。
 */

typedef struct {
    long long bits;
    long long send_ns;
    long long half_ns;
    long long prop_ns;
    long long dev_ns;
    long long meters;
} Span;

typedef struct {
    int collisions;
    int k;
    int rmax;
    long long slot_ns;
    long long max_ns;
} Backoff;

typedef struct {
    long long frame_bits;
    long long ack_bits;
    long long data_bits;
    long long bps;
    long long tau_ns;
    long long frame_ns;
    long long ack_ns;
    long long period_ns;
    long long out_bps;
    int centi;
    int exact;
} Goodput;

bool parse_speed(double value, const char *unit, long long *mps);
bool span_times(long long bits, long long bps, long long *send_ns, long long *half_ns);
bool span_by_distance(long long bps, long long meters, long long mps, long long dev_ns, Span *o);
bool span_by_frame(long long bits, long long bps, long long mps, long long dev_ns, Span *o);
bool span_delta(long long bits, long long bps, long long mps, long long *rtt_ns, long long *one_ns,
                long long *meters);
bool span_prop(long long meters, long long mps, long long *tau_ns);
bool backoff_calc(int collisions, long long slot_ns, Backoff *o);
bool backoff_chosen(int collisions, int r, long long slot_ns, Backoff *o, long long *wait_ns);
bool goodput_calc(Goodput *g);

#endif
