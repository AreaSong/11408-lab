#ifndef FLOWCTL_H
#define FLOWCTL_H

typedef struct {
    long mss;
    long cwnd;
    long rwnd_adv;
    int rwnd_known;
    long buf;
    long buffered;
    long in_flight;
    long next_seq;
    int seq_set;
    int persist;
} Flow;

void flow_clear(Flow *f);
const char *flow_set_mss(Flow *f, long mss);
const char *flow_set_cwnd(Flow *f, long cwnd);
const char *flow_set_buf(Flow *f, long buf);
const char *flow_set_seq(Flow *f, long seq);

long flow_swnd(const Flow *f);
long flow_can_send(const Flow *f);
long flow_true_rwnd(const Flow *f);

const char *flow_send(Flow *f, long n);
const char *flow_ack(Flow *f, long n, int has_rwnd, long rwnd);
const char *flow_read(Flow *f, long n, int lose);
const char *flow_probe(Flow *f, int *accepted);

#endif
