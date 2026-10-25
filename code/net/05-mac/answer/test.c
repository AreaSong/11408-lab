#include "bus.h"
#include "cdma.h"
#include "limit.h"
#include "qty.h"
#include "test.h"

#include <string.h>

static void test_units(void) {
    long long v = 0;

    CHECK(parse_ns(51.2, "us", &v));
    CHECK_EQ(v, 51200);
    CHECK(parse_ns(1.535, "us", &v));
    CHECK_EQ(v, 1535);
    CHECK(parse_bits(64, "B", &v));
    CHECK_EQ(v, 512);
    CHECK(parse_bits(128, "B", &v));
    CHECK_EQ(v, 1024);
    CHECK(parse_bits(1518, "B", &v));
    CHECK_EQ(v, 12144);
    CHECK(parse_bits(1500, "B", &v));
    CHECK_EQ(v, 12000);
    CHECK(parse_bps(100, "Mbps", &v));
    CHECK_EQ(v, 100000000LL);
    CHECK(parse_bps(1, "Gbps", &v));
    CHECK_EQ(v, 1000000000LL);
    CHECK(parse_bps(10, "Mbps", &v));
    CHECK_EQ(v, 10000000LL);
    CHECK(parse_meters(2, "km", &v));
    CHECK_EQ(v, 2000);
    CHECK(parse_speed(200, "m/us", &v));
    CHECK_EQ(v, 200000000LL);
    CHECK(parse_speed(200000, "km/s", &v));
    CHECK_EQ(v, 200000000LL);
    CHECK(parse_speed(200000000, "m/s", &v));
    CHECK_EQ(v, 200000000LL);
}

static void test_cdma(void) {
    int a[] = {1, 1, 1, 1};
    int b[] = {1, -1, 1, -1};
    int d[] = {1, 1, -1, -1};
    int bad[] = {1, 1, 1, -1};
    int recv[] = {2, 0, 2, 0, 0, -2, 0, -2, 0, 2, 0, 2};
    int norm = 0;
    int ia = 0;
    int ib = 0;
    int dot = 0;
    char out[16];
    Cdma net;
    Cdma skew;

    CHECK_EQ(cdma_dot(a, b, 4), 0);
    CHECK_EQ(cdma_dot(a, d, 4), 0);
    CHECK_EQ(cdma_dot(b, d, 4), 0);
    CHECK_EQ(cdma_dot(a, a, 4), 4);
    CHECK(cdma_norm(4, 4, &norm) && norm == 1);
    CHECK(cdma_norm(-4, 4, &norm) && norm == -1);
    CHECK(cdma_norm(0, 4, &norm) && norm == 0);
    CHECK(!cdma_norm(2, 4, &norm));

    cdma_init(&net);
    CHECK(cdma_add(&net, "A", a, 4));
    CHECK(cdma_add(&net, "B", b, 4));
    CHECK(cdma_add(&net, "C", d, 4));
    CHECK(cdma_orthogonal(&net, &ia, &ib, &dot));
    CHECK(cdma_set_send(&net, "A", "101"));
    CHECK(cdma_set_send(&net, "B", "110"));
    CHECK(cdma_set_send(&net, "C", "xxx"));
    CHECK(cdma_mix(&net));
    CHECK_EQ(net.nrecv, 12);
    CHECK(memcmp(net.recv, recv, sizeof recv) == 0);
    CHECK(cdma_decode(&net, 0, out, (int)sizeof out));
    CHECK_STR(out, "101");
    CHECK(cdma_decode(&net, 1, out, (int)sizeof out));
    CHECK_STR(out, "110");
    CHECK(cdma_decode(&net, 2, out, (int)sizeof out));
    CHECK_STR(out, "xxx");

    cdma_init(&skew);
    CHECK(cdma_add(&skew, "A", a, 4));
    CHECK(cdma_add(&skew, "Z", bad, 4));
    CHECK(!cdma_orthogonal(&skew, &ia, &ib, &dot));
    CHECK_EQ(dot, 2);
}

static void test_span(void) {
    Span sp;
    long long rtt = 0;
    long long one = 0;
    long long meters = 0;
    long long tau = 0;
    long long send = 0;
    long long half = 0;
    Goodput g;

    CHECK(span_by_distance(10000000LL, 2500, 200000000LL, 0, &sp));
    CHECK_EQ(sp.bits, 250);
    CHECK_EQ(sp.send_ns, 25000);
    CHECK(span_by_frame(512, 10000000LL, 200000000LL, 0, &sp));
    CHECK_EQ(sp.meters, 5120);
    CHECK_EQ(sp.send_ns, 51200);
    CHECK(span_by_frame(512, 100000000LL, 200000000LL, 1535, &sp));
    CHECK_EQ(sp.half_ns, 2560);
    CHECK_EQ(sp.prop_ns, 1025);
    CHECK_EQ(sp.meters, 205);
    CHECK(span_times(1024, 100000000LL, &send, &half));
    CHECK_EQ(send, 10240);
    CHECK_EQ(half, 5120);
    CHECK(span_delta(800, 1000000000LL, 200000000LL, &rtt, &one, &meters));
    CHECK_EQ(rtt, 800);
    CHECK_EQ(one, 400);
    CHECK_EQ(meters, 80);
    CHECK(span_prop(2000, 200000000LL, &tau));
    CHECK_EQ(tau, 10000);

    memset(&g, 0, sizeof g);
    g.frame_bits = 12144;
    g.ack_bits = 512;
    g.data_bits = 12000;
    g.bps = 10000000LL;
    g.tau_ns = 10000;
    CHECK(goodput_calc(&g));
    CHECK_EQ(g.frame_ns, 1214400);
    CHECK_EQ(g.ack_ns, 51200);
    CHECK_EQ(g.period_ns, 1285600);
    CHECK_EQ(g.out_bps, 12000LL * 1000000000LL / 1285600LL);
    CHECK_EQ(g.centi, 933);
    CHECK(!g.exact);
}

static void test_backoff(void) {
    Backoff b;
    long long wait = 0;
    int k = 0;
    int rmax = 0;

    CHECK(backoff_calc(4, 51200, &b));
    CHECK_EQ(b.k, 4);
    CHECK_EQ(b.rmax, 15);
    CHECK_EQ(b.max_ns, 768000);
    CHECK(backoff_calc(12, 51200, &b));
    CHECK_EQ(b.k, 10);
    CHECK_EQ(b.rmax, 1023);
    CHECK(backoff_calc(16, 51200, &b));
    CHECK_EQ(b.rmax, -1);
    CHECK(backoff_chosen(2, 3, 51200, &b, &wait));
    CHECK_EQ(wait, 153600);
    CHECK(!backoff_chosen(1, 3, 51200, &b, &wait));
    CHECK_EQ(bus_pick(0, 1, &k, &rmax), 0);
    CHECK_EQ(k, 1);
    CHECK_EQ(rmax, 1);
    CHECK_EQ(bus_pick(1, 1, NULL, NULL), 1);
    CHECK_EQ(bus_pick(2, 1, NULL, NULL), 0);
    CHECK_EQ(bus_pick(0, 2, NULL, NULL), 1);
    CHECK_EQ(bus_pick(2, 2, NULL, NULL), 3);
    CHECK_EQ(bus_pick(0, 3, NULL, NULL), 2);
    CHECK_EQ(bus_pick(2, 3, NULL, NULL), 4);
    CHECK_EQ(bus_pick(0, 12, &k, &rmax), (17 * 0 + 29 * 12 + 3) % 1024);
    CHECK_EQ(k, 10);
    CHECK_EQ(rmax, 1023);
    CHECK_EQ(bus_pick(0, 16, NULL, NULL), -1);
}

static void test_bus(void) {
    BusStat st;
    int n;

    CHECK(bus_run_csma(2, 1, 4, NULL, &st));
    CHECK(st.done);
    CHECK_EQ(st.useful, 8);
    CHECK_EQ(st.total, 9);
    CHECK_EQ(st.collisions, 1);
    CHECK_EQ(st.dropped, 0);
    CHECK(bus_run_csma(3, 1, 4, NULL, &st));
    CHECK_EQ(st.useful, 12);
    CHECK_EQ(st.total, 17);
    CHECK_EQ(st.collisions, 3);
    CHECK_EQ(st.dropped, 0);
    CHECK(bus_run_token(3, 1, 4, 1, NULL, &st));
    CHECK_EQ(st.useful, 12);
    CHECK_EQ(st.total, 14);
    CHECK_EQ(st.collisions, 0);
    for (n = 2; n <= 20; n++) {
        CHECK(bus_run_csma(n, 2, 8, NULL, &st));
        CHECK(st.done);
        CHECK_EQ(st.dropped, 0);
        CHECK_EQ(st.useful, (long long)n * 2 * 8);
        CHECK(bus_run_token(n, 2, 8, 1, NULL, &st));
        CHECK_EQ(st.useful, (long long)n * 2 * 8);
        CHECK_EQ(st.total, st.useful + ((long long)n * 2 - 1));
    }
}

int main(void) {
    test_units();
    test_cdma();
    test_span();
    test_backoff();
    test_bus();
    return TEST_SUMMARY();
}
