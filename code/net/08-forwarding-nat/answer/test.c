#include "addr.h"
#include "arp.h"
#include "cksum.h"
#include "ipv6.h"
#include "nat.h"
#include "pkt.h"
#include "test.h"
#include "trie.h"

#include <string.h>

static void check_checksum(void) {
    unsigned char orig[20] = {0x45, 0x00, 0x01, 0xef, 0x11, 0x3b, 0x40, 0x00, 0x80, 0x06,
                              0xba, 0x9d, 0x0a, 0x02, 0x80, 0x64, 0x40, 0xaa, 0x62, 0x20};
    unsigned char ttl1[20] = {0x45, 0x00, 0x00, 0x1c, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01,
                              0xa5, 0xde, 0x0a, 0x00, 0x00, 0x01, 0x0a, 0x00, 0x00, 0x02};
    IpView view;
    FwdResult fwd;
    char err[80];

    CHECK_EQ(ip_checksum(orig, 20), 0xba9du);
    CHECK(ip_load(orig, 20, &view, err, (int)sizeof err));
    CHECK(ip_forward(&view, 1, 0x650c7b0fu, &fwd));
    CHECK(!fwd.dropped);
    CHECK_EQ(fwd.new_ttl, 127);
    CHECK_EQ(fwd.new_sum, 0x65e8u);
    CHECK_EQ(fwd.new_src, 0x650c7b0fu);
    CHECK(ip_load(ttl1, 20, &view, err, (int)sizeof err));
    CHECK(ip_forward(&view, 0, 0, &fwd));
    CHECK(fwd.dropped);
    CHECK_EQ(fwd.new_ttl, 0);
}

static void check_trie(void) {
    Fib fib;
    char err[80];
    uint32_t ip = 0;

    fib_init(&fib);
    CHECK(ip_parse("194.17.20.0", &ip));
    CHECK(fib_add(&fib, ip, 23, 0, 0xc2111802u, "S1", err, (int)sizeof err));
    CHECK(ip_parse("194.17.20.128", &ip));
    CHECK(fib_add(&fib, ip, 25, 1, 0, "E0", err, (int)sizeof err));
    CHECK(ip_parse("194.17.20.200", &ip));
    CHECK_EQ(fib.item[fib_lookup(&fib, ip)].prefix, 25);
    CHECK(ip_parse("194.17.21.1", &ip));
    CHECK_EQ(fib.item[fib_lookup(&fib, ip)].prefix, 23);
    CHECK(fib_add(&fib, 0, 0, 0, 0xca760202u, "L0", err, (int)sizeof err));
    CHECK(ip_parse("8.8.8.8", &ip));
    CHECK_EQ(fib.item[fib_lookup(&fib, ip)].prefix, 0);
    CHECK_EQ(fib_lookup(&fib, ip) >= 0, 1);
    fib_clear(&fib);
}

static void check_nat(void) {
    NatBox box;
    Flow flow;
    char err[80];

    natbox_init(&box, "gw");
    CHECK(natbox_pool(&box, 0xac100001u, 40000, 40007, err, (int)sizeof err));
    memset(&flow, 0, sizeof flow);
    flow.proto = NAT_TCP;
    flow.src = 0xc0a80002u;
    flow.sport = 1234;
    flow.dst = 0x01020304u;
    flow.dport = 80;
    CHECK(natbox_out(&box, &flow, err, (int)sizeof err));
    CHECK_EQ(flow.src, 0xac100001u);
    CHECK_EQ(flow.sport, 40000);
    flow.src = 0xc0a80003u;
    flow.sport = 1234;
    flow.dst = 0x01020304u;
    flow.dport = 80;
    CHECK(natbox_out(&box, &flow, err, (int)sizeof err));
    CHECK_EQ(flow.sport, 40001);
    memset(&flow, 0, sizeof flow);
    flow.proto = NAT_TCP;
    flow.src = 0x01020304u;
    flow.sport = 80;
    flow.dst = 0xac100001u;
    flow.dport = 40000;
    CHECK(natbox_in(&box, &flow, err, (int)sizeof err));
    CHECK_EQ(flow.dst, 0xc0a80002u);
    CHECK_EQ(flow.dport, 1234);
}

static void check_arp(void) {
    ArpCache cache;
    uint8_t mac[6] = {0x00, 0x21, 0x27, 0x21, 0x51, 0xee};
    uint8_t got[6];
    int remote = 0;
    VlanHost tab[VLAN_MAX];
    int n = 0;

    memset(&cache, 0, sizeof cache);
    CHECK_EQ(arp_target(0x0a028064u, 24, 0x40aa6220u, 0x0a028001u, &remote), 0x0a028001u);
    CHECK(remote);
    remote = 1;
    CHECK_EQ(arp_target(0x0a028064u, 24, 0x0a028014u, 0x0a028001u, &remote), 0x0a028014u);
    CHECK(!remote);
    CHECK(arp_put(&cache, 0x0a028001u, mac));
    CHECK(arp_get(&cache, 0x0a028001u, got));
    CHECK(memcmp(got, mac, 6) == 0);
    CHECK(vlan_add(tab, &n, 1, 0xc0a80351u, mac));
    CHECK(vlan_add(tab, &n, 3, 0xc0a80381u, mac));
    CHECK_EQ(vlan_find(tab, n, 0xc0a80381u), 3);
}

static void check_ipv6(void) {
    uint16_t group[8];
    char out[64];
    uint16_t again[8];

    CHECK(ipv6_parse("2001:0db8:0000:0000:0000:0000:0000:0001", group));
    CHECK(ipv6_compress(group, out, (int)sizeof out));
    CHECK_STR(out, "2001:db8::1");
    CHECK(ipv6_parse("2001:0db8:ffff:0000:0123:4567:89ab:cdef", group));
    CHECK(ipv6_compress(group, out, (int)sizeof out));
    CHECK_STR(out, "2001:db8:ffff:0:123:4567:89ab:cdef");
    CHECK(ipv6_parse("2001:0000:0000:ffff:0000:0000:0001:0002", group));
    CHECK(ipv6_compress(group, out, (int)sizeof out));
    CHECK_STR(out, "2001::ffff:0:0:1:2");
    CHECK(ipv6_parse("::", group));
    CHECK(ipv6_compress(group, out, (int)sizeof out));
    CHECK_STR(out, "::");
    CHECK(ipv6_parse("ff05::b3", group));
    CHECK(ipv6_expand(group, out, (int)sizeof out));
    CHECK_STR(out, "ff05:0000:0000:0000:0000:0000:0000:00b3");
    CHECK(ipv6_parse(out, again));
    CHECK(memcmp(group, again, sizeof group) == 0);
    CHECK(!ipv6_parse("1::2::3", group));
    CHECK(!ipv6_parse("2001:db8", group));
}

int main(void) {
    check_checksum();
    check_trie();
    check_nat();
    check_arp();
    check_ipv6();
    return TEST_SUMMARY();
}
