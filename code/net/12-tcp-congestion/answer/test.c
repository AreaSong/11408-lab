#include "cong.h"
#include "flowctl.h"
#include "rtt.h"
#include "test.h"
#include "units.h"

static void check_units(void) {
    long n = 0;

    CHECK(parse_bytes("1KB", &n) == 0);
    CHECK_EQ(n, 1024);
    CHECK(parse_bytes("1000", &n) == 0);
    CHECK_EQ(n, 1000);
    CHECK(parse_time_us("1.5s", &n) == 0);
    CHECK_EQ(n, 1500000);
    CHECK(parse_time_us("2.5s", &n) == 0);
    CHECK_EQ(n, 2500000);
    CHECK(parse_time_us("200ms", &n) == 0);
    CHECK_EQ(n, 200000);
    CHECK(parse_time_us("5ms", &n) == 0);
    CHECK_EQ(n, 5000);
}

static void round_n(Cong *c, int n) {
    RoundSnap snap;
    int i;

    for (i = 0; i < n; i++) {
        CHECK(cong_round(c, &snap) == NULL);
    }
}

static void check_2009(void) {
    Cong c;
    long old = 0;

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_init(&c, 16 * 1024, 32 * 1024) == NULL);
    CHECK(cong_timeout(&c, &old) == NULL);
    CHECK_EQ(old, 16 * 1024);
    CHECK_EQ(c.ssthresh, 8 * 1024);
    CHECK_EQ(c.cwnd, 1024);
    round_n(&c, 4);
    CHECK_EQ(c.cwnd, 9 * 1024);
    CHECK_EQ(c.rounds, 4);
}

static void check_2014(void) {
    Cong c;
    long old = 0;

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_set_rwnd(&c, 10 * 1024) == NULL);
    CHECK(cong_init(&c, 8 * 1024, 8 * 1024) == NULL);
    CHECK(cong_timeout(&c, &old) == NULL);
    CHECK_EQ(c.ssthresh, 4 * 1024);
    round_n(&c, 10);
    CHECK_EQ(c.cwnd, 12 * 1024);
    CHECK_EQ(cong_swnd(&c), 10 * 1024);
}

static void check_2015(void) {
    Cong c;

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_set_buf(&c, 16 * 1024) == NULL);
    CHECK(cong_init(&c, 1024, 32 * 1024) == NULL);
    round_n(&c, 4);
    CHECK_EQ(c.cwnd, 16 * 1024);
    CHECK_EQ(c.buffered, 15 * 1024);
    CHECK_EQ(cong_rwnd(&c), 1024);
    CHECK_EQ(cong_swnd(&c), 1024);
}

static void check_2016_ack(void) {
    Cong c;
    long sent = 0;

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_set_buf(&c, 20 * 1024) == NULL);
    CHECK(cong_set_seq(&c, 101) == NULL);
    CHECK(cong_init(&c, 1024, 32 * 1024) == NULL);
    round_n(&c, 3);
    CHECK_EQ(c.new_acks, 7);
    CHECK_EQ(c.cwnd, 8 * 1024);
    CHECK(cong_send(&c, &sent) == NULL);
    CHECK_EQ(sent, 8 * 1024);
    CHECK(cong_ack(&c, 1) == NULL);
    CHECK_EQ(c.new_acks, 8);
    CHECK_EQ(c.cwnd, 9 * 1024);
    CHECK_EQ(cong_rwnd(&c), 12 * 1024);
    CHECK_EQ(cong_swnd(&c), 9 * 1024);
}

static void check_2016_stall(void) {
    Cong c;
    RoundSnap snaps[CONG_SNAP_MAX];
    StallInfo info;

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_set_rtt(&c, 200000) == NULL);
    CHECK(cong_set_buf(&c, 20 * 1024) == NULL);
    CHECK(cong_set_seq(&c, 101) == NULL);
    CHECK(cong_init(&c, 1024, 32 * 1024) == NULL);
    CHECK(cong_stall(&c, snaps, CONG_SNAP_MAX, &info) == NULL);
    CHECK_EQ(info.rounds, 5);
    CHECK_EQ(info.bytes, 20 * 1024);
    CHECK_EQ(info.next_seq, 20581);
    CHECK_EQ(cong_swnd(&c), 0);
}

static void check_until(void) {
    Cong c;
    RoundSnap snaps[CONG_SNAP_MAX];
    int n = 0;
    long old = 0;

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_set_rtt(&c, 5000) == NULL);
    CHECK(cong_set_buf(&c, 64 * 1024) == NULL);
    CHECK(cong_init(&c, 1024, 64 * 1024) == NULL);
    CHECK(cong_until(&c, UNTIL_SWND, 32 * 1024, snaps, CONG_SNAP_MAX, &n) == NULL);
    CHECK_EQ(n, 5);
    CHECK_EQ(cong_swnd(&c), 32 * 1024);

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_set_rtt(&c, 2000) == NULL);
    CHECK(cong_init(&c, 8 * 1024, 8 * 1024) == NULL);
    CHECK(cong_until(&c, UNTIL_CWND, 32 * 1024, snaps, CONG_SNAP_MAX, &n) == NULL);
    CHECK_EQ(n, 24);
    CHECK_EQ(c.cwnd, 32 * 1024);

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_init(&c, 16 * 1024, 32 * 1024) == NULL);
    CHECK(cong_timeout(&c, &old) == NULL);
    CHECK(cong_until(&c, UNTIL_CWND, 16 * 1024, snaps, CONG_SNAP_MAX, &n) == NULL);
    CHECK_EQ(n, 11);
    CHECK_EQ(c.cwnd, 16 * 1024);
    CHECK_EQ(c.ssthresh, 8 * 1024);
}

static void check_cap_and_fast(void) {
    Cong c;
    RoundSnap snap;
    int fired = 0;
    long old = 0;

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_init(&c, 8 * 1024, 12 * 1024) == NULL);
    CHECK(cong_round(&c, &snap) == NULL);
    CHECK_EQ(c.cwnd, 12 * 1024);
    CHECK(snap.capped);

    cong_clear(&c);
    CHECK(cong_set_mss(&c, 1024) == NULL);
    CHECK(cong_init(&c, 16 * 1024, 32 * 1024) == NULL);
    CHECK(cong_dup(&c, &fired, &old) == NULL);
    CHECK_EQ(fired, 0);
    CHECK(cong_dup(&c, &fired, &old) == NULL);
    CHECK(cong_dup(&c, &fired, &old) == NULL);
    CHECK_EQ(fired, 1);
    CHECK_EQ(c.ssthresh, 8 * 1024);
    CHECK_EQ(c.cwnd, 8 * 1024);
}

static void check_rto(void) {
    RttEst e;
    RttStep step;
    long rto = 0;

    rtt_clear(&e);
    CHECK(rtt_set_rto(&e, 6000000) == NULL);
    CHECK(rtt_sample(&e, 1500000, &step) == NULL);
    CHECK(step.first);
    CHECK_EQ(step.rto_us, 4500000);
    CHECK_EQ(step.rttd_us, 750000);
    CHECK(rtt_sample(&e, 2500000, &step) == NULL);
    CHECK_EQ(step.rtts_us, 1625000);
    CHECK_EQ(step.rttd_us, 781250);
    CHECK_EQ(step.rto_us, 4750000);
    CHECK_EQ(step.rfc_rto_us, 4875000);
    CHECK(rtt_rexmit(&e, &rto) == NULL);
    CHECK_EQ(rto, 9500000);
    CHECK(rtt_sample(&e, 3000000, &step) == NULL);
    CHECK(step.ignored);
    CHECK_EQ(step.rto_us, 9500000);
    CHECK_EQ(e.rtts_us, 1625000);
}

static void check_flow(void) {
    Flow f;
    int accepted = 0;

    flow_clear(&f);
    CHECK(flow_set_mss(&f, 1000) == NULL);
    CHECK(flow_set_cwnd(&f, 4000) == NULL);
    CHECK(flow_send(&f, 1000) == NULL);
    CHECK(flow_send(&f, 1000) == NULL);
    CHECK(flow_ack(&f, 1000, 1, 2000) == NULL);
    CHECK_EQ(f.in_flight, 1000);
    CHECK_EQ(flow_swnd(&f), 2000);
    CHECK_EQ(flow_can_send(&f), 1000);

    flow_clear(&f);
    CHECK(flow_set_cwnd(&f, 1000) == NULL);
    CHECK(flow_set_buf(&f, 300) == NULL);
    CHECK(flow_set_seq(&f, 1) == NULL);
    CHECK(flow_send(&f, 100) == NULL);
    CHECK(flow_ack(&f, 100, 0, 0) == NULL);
    CHECK(flow_send(&f, 100) == NULL);
    CHECK(flow_ack(&f, 100, 0, 0) == NULL);
    CHECK(flow_send(&f, 100) == NULL);
    CHECK(flow_ack(&f, 100, 0, 0) == NULL);
    CHECK(f.persist);
    CHECK_EQ(flow_true_rwnd(&f), 0);
    CHECK(flow_send(&f, 100) != NULL);
    CHECK(flow_read(&f, 300, 1) == NULL);
    CHECK_EQ(f.rwnd_adv, 0);
    CHECK(flow_probe(&f, &accepted) == NULL);
    CHECK(accepted);
    CHECK_EQ(f.rwnd_adv, 299);
    CHECK_EQ(f.next_seq, 302);
    CHECK(!f.persist);
}

int main(void) {
    check_units();
    check_2009();
    check_2014();
    check_2015();
    check_2016_ack();
    check_2016_stall();
    check_until();
    check_cap_and_fast();
    check_rto();
    check_flow();
    return TEST_SUMMARY();
}
