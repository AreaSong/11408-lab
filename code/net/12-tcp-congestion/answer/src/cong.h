#ifndef CONG_H
#define CONG_H

/* 一轮结束时的窗口。cwnd 用字节，打印时再换成 KB。 */
#define CONG_SNAP_MAX 64

typedef struct {
    int index;
    int slow;
    int capped;
    long sent;
    long cwnd0;
    long cwnd1;
    long ssthresh;
    long rwnd1;
    long swnd1;
    int limited;
    int use_buf;
    long buffered1;
} RoundSnap;

typedef struct {
    long mss;
    long cwnd;
    long ssthresh;
    long rwnd;
    int rwnd_fixed;
    long buf;
    long buffered;
    long in_flight;
    long next_seq;
    int seq_set;
    long rtt_us;
    int rounds;
    int new_acks;
    int dup_acks;
    int ready;
    long ca_base;
    long ca_credit;
} Cong;

typedef struct {
    int rounds;
    long bytes;
    long next_seq;
    int seq_set;
    long rtt_us;
    int snap_n;
} StallInfo;

enum { UNTIL_CWND = 0, UNTIL_SWND = 1 };

void cong_clear(Cong *c);

const char *cong_set_mss(Cong *c, long mss);
const char *cong_set_rtt(Cong *c, long us);
const char *cong_set_buf(Cong *c, long n);
const char *cong_set_rwnd(Cong *c, long n);
const char *cong_set_seq(Cong *c, long seq);
const char *cong_init(Cong *c, long cwnd, long ssthresh);

long cong_rwnd(const Cong *c);
long cong_swnd(const Cong *c);

const char *cong_timeout(Cong *c, long *old_cwnd);
const char *cong_round(Cong *c, RoundSnap *snap);
const char *cong_send(Cong *c, long *sent);
const char *cong_ack(Cong *c, int n);
const char *cong_dup(Cong *c, int *fired, long *old_cwnd);

const char *cong_until(Cong *c, int kind, long target, RoundSnap *snaps, int cap,
                       int *n_out);
const char *cong_stall(Cong *c, RoundSnap *snaps, int cap, StallInfo *info);

#endif
