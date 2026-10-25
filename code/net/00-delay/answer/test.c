#include <string.h>

#include "delay.h"
#include "exchange.h"
#include "qty.h"
#include "route.h"
#include "test.h"

static void test_units(void) {
    long long v = 0;

    CHECK(parse_bits(8, "Mb", &v));
    CHECK_EQ(v, 8000000LL);
    CHECK(parse_bits(10, "kb", &v));
    CHECK_EQ(v, 10000LL);
    CHECK(parse_bits(1, "MB", &v));
    CHECK_EQ(v, 8000000LL);
    CHECK(parse_bits(1000, "B", &v));
    CHECK_EQ(v, 8000LL);
    CHECK(parse_bps(100, "Mbps", &v));
    CHECK_EQ(v, 100000000LL);
    CHECK(parse_bps(1, "Mbps", &v));
    CHECK_EQ(v, 1000000LL);
    /* 1 M 在计网带宽里是 10^6，存储容量里的 2^20 不能拿来用 */
    CHECK(v != (1LL << 20));
    CHECK(parse_bps(10, "kbps", &v));
    CHECK_EQ(v, 10000LL);
    CHECK(!parse_bps(1, "MB/s", &v));
    CHECK(parse_ns(0.01, "ms", &v));
    CHECK_EQ(v, 10000LL);
    CHECK(parse_meters(1000, "km", &v));
    CHECK_EQ(v, 1000000LL);
}

static void test_link_util_throughput(void) {
    LinkOut link;
    UtilOut util;
    long long rates[3];
    long long best = 0;
    long long good = 0;

    CHECK(link_compute(8000, 10000000LL, 1000000LL, 200000000LL, 0, 0, &link));
    CHECK_EQ(link.tx_ns, 800000LL);
    CHECK_EQ(link.prop_ns, 5000000LL);
    CHECK_EQ(link.total_ns, 5800000LL);
    CHECK_EQ(link.rtt_ns, 10000000LL);
    CHECK_EQ(link.bdp_bits, 50000LL);

    /* 2020-36：Td = Ta = 800 ms，往返传播 400 ms，800/2000 = 40% */
    CHECK(util_compute(8000, 8000, 10000, 200000000LL, &util));
    CHECK_EQ(util.td_ns, 800000000LL);
    CHECK_EQ(util.ta_ns, 800000000LL);
    CHECK_EQ(util.rtt_ns, 400000000LL);
    CHECK_EQ(util.cycle_ns, 2000000000LL);
    CHECK_EQ(util.td_ns * 100 / util.cycle_ns, 40);

    rates[0] = 100000000LL;
    rates[1] = 10000000LL;
    rates[2] = 1000000000LL;
    CHECK(bottleneck_bps(rates, 3, &best));
    CHECK_EQ(best, 10000000LL);
    CHECK(goodput_bps(980 * 8, 20 * 8, 100000000LL, &good));
    CHECK_EQ(good, 98000000LL);
}

static void test_2010_2013_2023(void) {
    ExchangeIn in;
    ExchangeOut out;

    /* 2010-34：3 段链路，1000 个分组，每个 0.08 ms，总时延 80.16 ms */
    in.hops = 3;
    in.bps = 100000000LL;
    in.prop_ns = 0;
    in.file_bits = 980000LL * 8;
    in.packet_bits = 8000;
    in.header_bits = 160;
    CHECK(exchange_compute(&in, 0, &out));
    CHECK_EQ(out.packets, 1000);
    CHECK(out.uniform);
    CHECK_EQ(out.packet_ns, 1002LL * 80000LL);

    /* 2013-35：报文 1600 ms，分组 801 ms；建立时延为 0 时电路交换是 800 ms */
    in.hops = 2;
    in.bps = 10000000LL;
    in.prop_ns = 0;
    in.file_bits = 8000000LL;
    in.packet_bits = 10000;
    in.header_bits = 0;
    CHECK(exchange_compute(&in, 0, &out));
    CHECK_EQ(out.packets, 800);
    CHECK_EQ(out.circuit_ns, 800000000LL);
    CHECK_EQ(out.message_ns, 1600000000LL);
    CHECK_EQ(out.packet_ns, 801000000LL);

    /* 2023-33：时延带宽积 1000 bit → 传播 0.01 ms，两段，总时延 80.10 ms */
    in.hops = 2;
    in.bps = 100000000LL;
    CHECK(qty_div_ns(1000, in.bps, &in.prop_ns));
    CHECK_EQ(in.prop_ns, 10000LL);
    in.file_bits = 8000000LL;
    in.packet_bits = 8000;
    in.header_bits = 0;
    CHECK(exchange_compute(&in, 0, &out));
    CHECK_EQ(out.packets, 1000);
    CHECK_EQ(out.packet_ns, 1001LL * 80000LL + 2LL * 10000LL);
}

static void test_short_last_packet(void) {
    ExchangeIn in;
    ExchangeOut out;

    /* 2500 bit 拆成 1000、1000、500。2 段、1000 bps、无传播：3.5 s */
    in.hops = 2;
    in.bps = 1000;
    in.prop_ns = 0;
    in.file_bits = 2500;
    in.packet_bits = 1000;
    in.header_bits = 0;
    CHECK(exchange_compute(&in, 0, &out));
    CHECK_EQ(out.packets, 3);
    CHECK(!out.uniform);
    CHECK_EQ(out.packet_ns, 3500000000LL);
}

static void set_link(RouteLink *e, const char *a, const char *b, long long ns) {
    snprintf(e->a, sizeof e->a, "%s", a);
    snprintf(e->b, sizeof e->b, "%s", b);
    e->ns = ns;
}

static void set_path(RoutePath *p, const char *x, const char *y, const char *z) {
    p->nodes = 3;
    snprintf(p->name[0], ROUTE_NAME, "%s", x);
    snprintf(p->name[1], ROUTE_NAME, "%s", y);
    snprintf(p->name[2], ROUTE_NAME, "%s", z);
}

static void test_route_order(void) {
    RouteLink links[4];
    RoutePath slow;
    RoutePath fast;
    long long slow_ns = 0;
    long long fast_ns = 0;
    long long path_ns[3];
    long long arrivals[3];
    int order[3];

    set_link(&links[0], "S", "A", 10000000LL);
    set_link(&links[1], "A", "D", 15000000LL);
    set_link(&links[2], "S", "B", 30000000LL);
    set_link(&links[3], "B", "D", 5000000LL);
    set_path(&slow, "S", "B", "D");
    set_path(&fast, "S", "A", "D");
    CHECK(route_path_ns(links, 4, &slow, &slow_ns));
    CHECK(route_path_ns(links, 4, &fast, &fast_ns));
    CHECK_EQ(slow_ns, 35000000LL);
    CHECK_EQ(fast_ns, 25000000LL);

    path_ns[0] = slow_ns;
    path_ns[1] = fast_ns;
    path_ns[2] = slow_ns;
    CHECK(route_arrivals(0, 5000000LL, path_ns, 3, arrivals));
    route_order(arrivals, 3, order);
    CHECK_EQ(order[0], 1);
    CHECK_EQ(order[1], 0);
    CHECK_EQ(order[2], 2);

    path_ns[0] = fast_ns;
    path_ns[1] = fast_ns;
    path_ns[2] = fast_ns;
    CHECK(route_arrivals(10000000LL, 5000000LL, path_ns, 3, arrivals));
    route_order(arrivals, 3, order);
    CHECK_EQ(order[0], 0);
    CHECK_EQ(order[1], 1);
    CHECK_EQ(order[2], 2);

    arrivals[0] = 10;
    arrivals[1] = 10;
    arrivals[2] = 5;
    route_order(arrivals, 3, order);
    CHECK_EQ(order[0], 2);
    CHECK_EQ(order[1], 0);
    CHECK_EQ(order[2], 1);
}

int main(void) {
    test_units();
    test_link_util_throughput();
    test_2010_2013_2023();
    test_short_last_packet();
    test_route_order();
    return TEST_SUMMARY();
}
