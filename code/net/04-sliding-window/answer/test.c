#include "limit.h"
#include "link.h"
#include "pipe.h"
#include "test.h"

#include <string.h>

static void expect_seq(const SeqList *list, const char *text) {
    char buf[128];

    CHECK(seq_text(list, buf, (int)sizeof buf));
    CHECK_STR(buf, text);
}

static void test_limits(void) {
    CHECK_EQ(limit_max_gbn(3), 7);
    CHECK_EQ(limit_max_sr_equal(3), 4);
    CHECK_EQ(limit_max_sr_recv(3, 5), 3);
    CHECK_EQ(limit_max_sr_recv(3, 4), 4);
    CHECK_EQ(limit_bits_gbn(11), 4);
    CHECK_EQ(limit_bits_gbn(7), 3);
    CHECK_EQ(limit_bits_gbn(8), 4);
    CHECK_EQ(limit_bits_sr_equal(8), 4);
    CHECK_EQ(limit_bits_sr_equal(4), 3);
    CHECK_EQ(limit_bits_sr_equal(7), 4);
    CHECK_EQ(limit_bits_slide(8), 4);
    CHECK_EQ(limit_bits_slide(7), 4);
    CHECK_EQ(limit_max_gbn(0), 0);
    CHECK_EQ(limit_max_sr_recv(3, 8), 0);
}

static int delays(long long frame, int same, long long ack, long long bps, long long prop,
                  PipeDelay *d) {
    char err[80];

    return pipe_delays(frame, ack, same, bps, prop, d, err, (int)sizeof err);
}

static void test_2012_and_2015(void) {
    PipeDelay d;

    CHECK(delays(1024, 1, 0, 16000, 270000000LL, &d));
    CHECK_EQ(d.td_ns, 64000000LL);
    CHECK_EQ(d.cycle_ns, 668000000LL);
    CHECK_EQ(pipe_window_for(&d, 100), 11);
    CHECK(delays(4096, 1, 0, 16000, 270000000LL, &d));
    CHECK_EQ(pipe_window_for(&d, 100), 5);
    CHECK_EQ(limit_bits_gbn(5), 3);

    CHECK(delays(8000, 0, 0, 128000, 250000000LL, &d));
    CHECK_EQ(d.td_ns, 62500000LL);
    CHECK_EQ(d.cycle_ns, 562500000LL);
    CHECK_EQ(pipe_window_for(&d, 80), 8);
    CHECK_EQ(pipe_window_for(&d, 100), 9);
}

static void test_2014_rate(void) {
    PipeDelay d;
    PipeRate r;
    char err[80];

    CHECK(delays(8000, 0, 0, 100000000LL, 50000000LL, &d));
    CHECK(pipe_rate(&d, 1000, &r, err, (int)sizeof err));
    CHECK_EQ(r.full_window, 1251);
    CHECK_EQ(r.saturated, 0);
    CHECK_EQ(r.util_milli, 79936);
    CHECK_EQ(r.rate_exact, 0);
    CHECK_EQ(r.rate_milli, 79936);
    CHECK_EQ(r.rate_whole, 80);
    CHECK_EQ(r.rate_approx, 1);

    CHECK(delays(1000, 0, 0, 10000, 200000000LL, &d));
    CHECK(pipe_rate(&d, 1, &r, err, (int)sizeof err));
    CHECK_EQ(r.util_milli, 20000);
    CHECK_EQ(r.rate_exact, 1);
    CHECK_EQ(r.rate_bps, 2000);
    CHECK_EQ(r.full_window, 5);
    CHECK(pipe_rate(&d, 5, &r, err, (int)sizeof err));
    CHECK_EQ(r.saturated, 1);
    CHECK_EQ(r.rate_bps, 10000);
    CHECK_EQ(r.util_milli, 100000);
}

static void test_2009(void) {
    Link link;
    SeqList sent;
    SeqList again;
    AckEvent ack;
    RecvEvent rv;
    char err[120];
    int i;

    CHECK(link_init(&link, LINK_GBN, 4, 8, -1, err, (int)sizeof err));
    CHECK(link_send(&link, 8, &sent, err, (int)sizeof err));
    expect_seq(&sent, "0 1 2 3 4 5 6 7");
    for (i = 0; i <= 7; i++) {
        CHECK(link_arrive(&link, i, &rv, err, (int)sizeof err));
        CHECK_EQ(rv.kind, RECV_ACCEPT);
    }
    CHECK(link_ack(&link, 0, &ack, err, (int)sizeof err));
    CHECK_EQ(link.base, 1);
    CHECK(link_ack(&link, 2, &ack, err, (int)sizeof err));
    expect_seq(&ack.slid, "1 2");
    CHECK_EQ(link.base, 3);
    CHECK(link_ack(&link, 3, &ack, err, (int)sizeof err));
    CHECK(link_timeout(&link, NULL, 0, &again, err, (int)sizeof err));
    expect_seq(&again, "4 5 6 7");
    CHECK_EQ(link.base, 4);
}

static void test_2011_and_2024(void) {
    Link link;
    SeqList sent;
    SeqList again;
    AckEvent ack;
    char err[120];
    int seqs[2];

    CHECK(link_init(&link, LINK_SR, 3, -1, -1, err, (int)sizeof err));
    CHECK_EQ(link.ws, 4);
    CHECK_EQ(link.wr, 4);
    CHECK(link_send(&link, 4, &sent, err, (int)sizeof err));
    CHECK(link_ack(&link, 1, &ack, err, (int)sizeof err));
    CHECK_EQ(ack.slid.n, 0);
    CHECK_EQ(link.base, 0);
    seqs[0] = 0;
    seqs[1] = 2;
    CHECK(link_timeout(&link, seqs, 2, &again, err, (int)sizeof err));
    expect_seq(&again, "0 2");
    CHECK(!link_timeout(&link, NULL, 0, &again, err, (int)sizeof err));

    CHECK(link_init(&link, LINK_SR, 3, -1, -1, err, (int)sizeof err));
    CHECK(link_send(&link, 4, &sent, err, (int)sizeof err));
    CHECK(link_ack(&link, 0, &ack, err, (int)sizeof err));
    CHECK_EQ(link.base, 1);
    CHECK(link_send(&link, 1, &sent, err, (int)sizeof err));
    expect_seq(&sent, "4");
    CHECK(link_ack(&link, 2, &ack, err, (int)sizeof err));
    CHECK_EQ(link.base, 1);
    seqs[0] = 1;
    CHECK(link_timeout(&link, seqs, 1, &again, err, (int)sizeof err));
    expect_seq(&again, "1");
    CHECK_EQ(link.win.data[link.win.front], link.base);
}

static void test_loss_and_fill(void) {
    Link link;
    SeqList sent;
    SeqList again;
    AckEvent ack;
    RecvEvent rv;
    char err[120];

    CHECK(link_init(&link, LINK_GBN, 3, 5, -1, err, (int)sizeof err));
    CHECK(link_send(&link, 5, &sent, err, (int)sizeof err));
    CHECK(link_arrive(&link, 0, &rv, err, (int)sizeof err));
    CHECK(link_ack(&link, 0, &ack, err, (int)sizeof err));
    CHECK(link_arrive(&link, 1, &rv, err, (int)sizeof err));
    CHECK(link_ack(&link, 1, &ack, err, (int)sizeof err));
    CHECK(link_arrive(&link, 3, &rv, err, (int)sizeof err));
    CHECK_EQ(rv.kind, RECV_DISCARD);
    CHECK_EQ(rv.ack, 1);
    CHECK(link_arrive(&link, 4, &rv, err, (int)sizeof err));
    CHECK(link_timeout(&link, NULL, 0, &again, err, (int)sizeof err));
    expect_seq(&again, "2 3 4");

    CHECK(link_init(&link, LINK_SR, 3, -1, -1, err, (int)sizeof err));
    CHECK(link_send(&link, 4, &sent, err, (int)sizeof err));
    CHECK(link_arrive(&link, 0, &rv, err, (int)sizeof err));
    CHECK(link_ack(&link, 0, &ack, err, (int)sizeof err));
    CHECK(link_arrive(&link, 2, &rv, err, (int)sizeof err));
    CHECK_EQ(rv.up.n, 0);
    CHECK(link_ack(&link, 2, &ack, err, (int)sizeof err));
    CHECK(link_arrive(&link, 3, &rv, err, (int)sizeof err));
    CHECK(link_ack(&link, 3, &ack, err, (int)sizeof err));
    CHECK(link_arrive(&link, 1, &rv, err, (int)sizeof err));
    expect_seq(&rv.up, "1 2 3");
    CHECK_EQ(link.expect, 4);
    CHECK(link_ack(&link, 1, &ack, err, (int)sizeof err));
    expect_seq(&ack.slid, "1 2 3");
    CHECK_EQ(link.base, 4);
    CHECK_EQ(cyc_count(&link.win), 0);
}

static void test_stop_and_reject(void) {
    Link link;
    SeqList sent;
    SeqList again;
    AckEvent ack;
    RecvEvent rv;
    char err[120];

    CHECK(link_init(&link, LINK_STOP, 1, -1, -1, err, (int)sizeof err));
    CHECK(link_send(&link, 1, &sent, err, (int)sizeof err));
    expect_seq(&sent, "0");
    CHECK(link_send(&link, 1, &sent, err, (int)sizeof err));
    CHECK_EQ(sent.n, 0);
    CHECK(link_arrive(&link, 0, &rv, err, (int)sizeof err));
    CHECK(link_ack(&link, 0, &ack, err, (int)sizeof err));
    CHECK(link_send(&link, 1, &sent, err, (int)sizeof err));
    expect_seq(&sent, "1");
    CHECK(link_timeout(&link, NULL, 0, &again, err, (int)sizeof err));
    expect_seq(&again, "1");
    CHECK(link_arrive(&link, 1, &rv, err, (int)sizeof err));
    CHECK(link_ack(&link, 1, &ack, err, (int)sizeof err));
    CHECK_EQ(link.base, 0);
    CHECK_EQ(link.next, 0);

    CHECK(!link_init(&link, LINK_GBN, 3, 8, -1, err, (int)sizeof err));
    CHECK(!link_init(&link, LINK_SR, 3, 5, 5, err, (int)sizeof err));
    CHECK(link_init(&link, LINK_SR, 3, 5, 3, err, (int)sizeof err));
    CHECK_EQ(link.ws, 5);
    CHECK_EQ(link.wr, 3);
}

int main(void) {
    test_limits();
    test_2012_and_2015();
    test_2014_rate();
    test_2009();
    test_2011_and_2024();
    test_loss_and_fill();
    test_stop_and_reject();
    return TEST_SUMMARY();
}
