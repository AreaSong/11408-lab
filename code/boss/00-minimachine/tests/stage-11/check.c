#include "net/net_arp.h"
#include "net/net_bgp.h"
#include "net/net_dhcp.h"
#include "net/net_fwd.h"
#include "net/net_ip.h"
#include "net/net_mcast.h"
#include "net/net_mip.h"
#include "net/net_nat.h"
#include "net/net_ospf.h"
#include "net/net_rip.h"
#include "net/net_sdn.h"
#include "net/net_stack.h"
#include "net/net_v6.h"
#include "test.h"

#include <string.h>

static void test_addr(void) {
    uint32_t addr = 0;
    int each = 0;
    char bcast[16];
    CHECK(net_ip_parse("10.1.2.3", &addr) == 0);
    CHECK_EQ(net_ip_class(addr), 'A');
    CHECK_EQ(net_ip_private(addr), 1);
    CHECK(net_ip_parse("172.32.1.1", &addr) == 0);
    CHECK_EQ(net_ip_class(addr), 'B');
    CHECK_EQ(net_ip_private(addr), 0);
    CHECK(net_ip_parse("172.31.1.1", &addr) == 0);
    CHECK_EQ(net_ip_private(addr), 1);
    CHECK(net_ip_parse("192.168.1.1", &addr) == 0);
    CHECK_EQ(net_ip_class(addr), 'C');
    CHECK(net_ip_parse("224.0.0.5", &addr) == 0);
    CHECK_EQ(net_ip_class(addr), 'D');
    CHECK(net_ip_parse("240.1.1.1", &addr) == 0);
    CHECK_EQ(net_ip_class(addr), 'E');
    CHECK_EQ(net_ip_fixed(24, 29, &each), 32);
    CHECK_EQ(each, 6);
    CHECK_EQ(net_ip_hosts(25), 126);
    CHECK_EQ(net_ip_hosts(30), 2);
    CHECK_EQ(net_ip_hosts(31), 0);
    CHECK(net_ip_parse("180.80.77.55", &addr) == 0);
    CHECK(net_ip_fmt(net_ip_bcast(addr, 22), bcast, (int)sizeof bcast) == 0);
    CHECK_STR(bcast, "180.80.79.255");
    CHECK_EQ(net_ip_hosts(22), 1022);
}

static void test_agg(void) {
    static const char *block[] = {"35.230.32.0/21", "35.230.40.0/21", "35.230.48.0/21", "35.230.56.0/21"};
    uint32_t nets[4];
    int pres[4];
    uint32_t agg = 0;
    uint32_t known = 0;
    uint32_t cand = 0;
    uint32_t other = 0;
    int out_pre = 0;
    int filled = 0;
    int ok = 0;
    int op = 0;
    char text[24];
    int i;
    uint32_t vlsm[5];
    int vpre[5];
    int hosts;
    for (i = 0; i < 4; i++) {
        CHECK(net_ip_cidr(block[i], &nets[i], &pres[i]) == 0);
    }
    CHECK(net_ip_agg(nets, pres, 4, &agg, &out_pre, &filled) == 0);
    CHECK_EQ(out_pre, 19);
    CHECK_EQ(filled, 1);
    CHECK(net_ip_fmt(agg, text, (int)sizeof text) == 0);
    CHECK_STR(text, "35.230.32.0");
    CHECK(net_ip_cidr("101.200.16.0/20", &agg, &out_pre) == 0);
    hosts = net_ip_vlsm(agg, out_pre, 5, vlsm, vpre, 5);
    CHECK_EQ(hosts, 254);
    CHECK_EQ(vpre[0], 21);
    CHECK_EQ(vpre[4], 24);
    CHECK(net_ip_cidr("192.168.9.128/26", &known, &out_pre) == 0);
    CHECK(net_ip_cidr("192.168.9.0/26", &cand, &op) == 0);
    CHECK(net_ip_third(known, out_pre, cand, op, 3, &other, &op, &ok) == 0);
    CHECK_EQ(ok, 0);
    CHECK(net_ip_cidr("192.168.9.0/25", &cand, &pres[0]) == 0);
    CHECK(net_ip_third(known, out_pre, cand, pres[0], 3, &other, &op, &ok) == 0);
    CHECK_EQ(ok, 1);
    CHECK_EQ(op, 26);
    CHECK(net_ip_fmt(other, text, (int)sizeof text) == 0);
    CHECK_STR(text, "192.168.9.192");
}

static void test_frag_hdr(void) {
    NetFrag piece[4];
    int sent[4];
    int hands[4];
    int pads[4];
    int ns = 0;
    int nh = 0;
    int np = 0;
    int app = 0;
    int routers = 0;
    int n = net_ip_frag(1580, 20, 800, 0, piece, 4);
    CHECK_EQ(n, 3);
    CHECK_EQ(piece[0].data, 776);
    CHECK_EQ(piece[1].total, 796);
    CHECK_EQ(piece[1].mf, 1);
    CHECK_EQ(piece[1].offset, 97);
    CHECK_EQ(piece[2].offset, 194);
    CHECK_EQ(piece[2].data, 8);
    n = net_ip_frag(1500, 20, 800, 0, piece, 4);
    CHECK_EQ(n, 2);
    CHECK_EQ(piece[1].total, 724);
    CHECK_EQ(piece[1].mf, 0);
    CHECK_EQ(net_ip_frag(1500, 20, 800, 1, piece, 4), -2);
    CHECK(net_ip_2012(sent, &ns, hands, &nh, pads, &np, &app, &routers) == 0);
    CHECK_EQ(ns, 3);
    CHECK_EQ(sent[0], 1);
    CHECK_EQ(sent[2], 4);
    CHECK_EQ(hands[2], 3);
    CHECK_EQ(pads[0], 3);
    CHECK_EQ(pads[1], 5);
    CHECK_EQ(app, 16);
    CHECK_EQ(routers, 15);
}

static void test_fwd(void) {
    NetFib fib;
    char hop[16];
    char iface[8];
    char changed[64];
    char src[16];
    uint32_t dst = 0;
    int ttl = 0;
    unsigned sum = 0;
    unsigned char raw[20] = {0x45, 0x00, 0x00, 0x1c, 0x00, 0x01, 0x00, 0x00, 0x40, 0x01,
                             0x66, 0xde, 0x0a, 0x00, 0x00, 0x01, 0x0a, 0x00, 0x00, 0x02};
    CHECK_EQ(net_ip_cksum(raw, 20), 0x66deu);
    CHECK(net_fib_load(&fib, "2013-47") == 0);
    CHECK(net_ip_parse("194.17.20.200", &dst) == 0);
    CHECK(net_fib_find(&fib, dst, hop, (int)sizeof hop, iface, (int)sizeof iface) == 25);
    CHECK_STR(hop, "直连");
    CHECK_STR(iface, "E0");
    CHECK(net_fib_load(&fib, "2009-47") == 0);
    CHECK(net_ip_parse("8.8.8.8", &dst) == 0);
    CHECK(net_fib_find(&fib, dst, hop, (int)sizeof hop, iface, (int)sizeof iface) == 0);
    CHECK_STR(hop, "202.118.2.2");
    CHECK_STR(iface, "L0");
    CHECK(net_ip_parse("202.118.3.2", &dst) == 0);
    CHECK(net_fib_find(&fib, dst, hop, (int)sizeof hop, iface, (int)sizeof iface) == 32);
    CHECK(net_fwd_2011(src, (int)sizeof src, &ttl, &sum, changed, (int)sizeof changed) == 0);
    CHECK_STR(src, "101.12.123.15");
    CHECK_EQ(ttl, 127);
    CHECK_EQ(sum, 0x65e8u);
    CHECK_STR(changed, "源地址、TTL、首部校验和");
}

static void test_arp_nat(void) {
    char target[16];
    char req[24];
    char mac[24];
    int vlan = 0;
    int show = 0;
    NetNat box;
    uint32_t pub = 0;
    uint32_t a = 0;
    uint32_t b = 0;
    int port = 0;
    NetArp cache;
    CHECK(net_arp_2011(target, (int)sizeof target, req, (int)sizeof req, mac, (int)sizeof mac) == 0);
    CHECK_STR(target, "10.2.128.1");
    CHECK_STR(req, "ff-ff-ff-ff-ff-ff");
    CHECK_STR(mac, "00-21-27-21-51-ee");
    CHECK(net_arp_2024("192.168.3.129", &vlan, &show) == 0);
    CHECK_EQ(vlan, 3);
    CHECK_EQ(show, 0);
    CHECK(net_arp_2024("192.168.3.81", &vlan, &show) == 0);
    CHECK_EQ(show, 1);
    memset(&cache, 0, sizeof cache);
    CHECK(net_ip_parse("10.0.0.1", &a) == 0);
    CHECK(net_arp_put(&cache, a, "02-00-00-00-01") == 0);
    CHECK(net_arp_get(&cache, a, mac, (int)sizeof mac) == 0);
    CHECK_STR(mac, "02-00-00-00-01");
    CHECK(net_ip_parse("203.0.113.10", &pub) == 0);
    CHECK(net_ip_parse("10.0.0.2", &a) == 0);
    CHECK(net_ip_parse("10.0.0.3", &b) == 0);
    net_nat_init(&box, pub);
    CHECK(net_nat_map(&box, a, 4000, &port) == 0);
    CHECK_EQ(port, 4000);
    CHECK(net_nat_map(&box, b, 4000, &port) == 0);
    CHECK_EQ(port, 4001);
}

static void test_route(void) {
    char hop[4][8];
    int cost[4];
    char via[8];
    char row[5][32];
    int dist = 0;
    int rounds = 0;
    int seconds = 0;
    int next = 0;
    CHECK(net_ospf_2014(hop, cost) == 0);
    CHECK_STR(hop[0], "直连");
    CHECK_EQ(cost[0], 1);
    CHECK_STR(hop[1], "R3");
    CHECK_EQ(cost[1], 3);
    CHECK_STR(hop[2], "R2");
    CHECK_EQ(cost[2], 4);
    CHECK_STR(hop[3], "R2");
    CHECK_EQ(cost[3], 8);
    CHECK_EQ(net_rip_rules(row), 5);
    CHECK_STR(row[0], "Net1 1 直连");
    CHECK_STR(row[1], "Net2 2 R2");
    CHECK_STR(row[2], "Net3 3 R2");
    CHECK_STR(row[3], "Net4 7 R2");
    CHECK_STR(row[4], "Net5 4 R2");
    CHECK(net_rip_2016(&dist, via, (int)sizeof via) == 0);
    CHECK_EQ(dist, 3);
    CHECK_STR(via, "R1");
    CHECK(net_rip_slow(0, &rounds) == 0);
    CHECK_EQ(rounds, 15);
    CHECK(net_rip_slow(1, &rounds) == 0);
    CHECK_EQ(rounds, 2);
    CHECK(net_rip_when(5, &seconds) == 0);
    CHECK_EQ(seconds, 60);
    CHECK(net_rip_when(3, &seconds) == 0);
    CHECK_EQ(seconds, 30);
    CHECK(net_rip_via(5, 0, &dist, &next) == 0);
    CHECK_EQ(dist, 4);
    CHECK_EQ(next, 1);
}

static void test_opt(void) {
    char text[80];
    char mac[24];
    char mac2[24];
    int alias = 0;
    int hop = 0;
    int state = 0;
    uint32_t addr = 0;
    unsigned sum = 0;
    unsigned after = 0;
    unsigned colo = 0;
    unsigned colo2 = 0;
    int path[] = {2, 1};
    CHECK(net_v6_compress("2001:0db8:0000:0000:0000:0000:0000:0001", text, (int)sizeof text) == 0);
    CHECK_STR(text, "2001:db8::1");
    CHECK(net_v6_compress("2001:db8:ffff:0:123:4567:89ab:cdef", text, (int)sizeof text) == 0);
    CHECK_STR(text, "2001:db8:ffff:0:123:4567:89ab:cdef");
    CHECK(net_v6_compress("2001:0:0:ffff:0:0:1:2", text, (int)sizeof text) == 0);
    CHECK_STR(text, "2001::ffff:0:0:1:2");
    CHECK(net_v6_compress("12ab:0:0:cd30:0:0:0:0", text, (int)sizeof text) == 0);
    CHECK_STR(text, "12ab:0:0:cd30::");
    CHECK(net_v6_expand("::1", text, (int)sizeof text) == 0);
    CHECK_STR(text, "0000:0000:0000:0000:0000:0000:0000:0001");
    CHECK(net_v6_expand("ff05::b3", text, (int)sizeof text) == 0);
    CHECK_STR(text, "ff05:0000:0000:0000:0000:0000:0000:00b3");
    CHECK(net_mcast_mac("224.215.145.230", mac, (int)sizeof mac, &alias) == 0);
    CHECK_STR(mac, "01-00-5e-57-91-e6");
    CHECK_EQ(alias, 32);
    CHECK(net_mcast_mac("224.0.1.1", mac, (int)sizeof mac, &alias) == 0);
    CHECK(net_mcast_mac("224.128.1.1", mac2, (int)sizeof mac2, &alias) == 0);
    CHECK_STR(mac, mac2);
    CHECK(net_mcast_mac("224.0.0.5", mac, (int)sizeof mac, &alias) == 0);
    CHECK_STR(mac, "01-00-5e-00-00-05");
    CHECK(net_mip_tunnel(&sum, &after, &colo, &colo2) == 0);
    CHECK_EQ(sum, 0x12b9u);
    CHECK_EQ(after, 0x13b9u);
    CHECK_EQ(colo, 0x1288u);
    CHECK_EQ(colo2, 0x1388u);
    CHECK_EQ(net_bgp_drop(path, 2, 1), 1);
    CHECK_EQ(net_bgp_drop(path, 2, 9), 0);
    CHECK(net_bgp_hop(14, text, (int)sizeof text) == 0);
    CHECK_STR(text, "R11");
    CHECK(net_bgp_hop(15, text, (int)sizeof text) == 0);
    CHECK_STR(text, "R13");
    CHECK(net_bgp_kind(4, 1, text, (int)sizeof text) == 0);
    CHECK_STR(text, "eBGP");
    CHECK(net_bgp_kind(1, 1, text, (int)sizeof text) == 0);
    CHECK_STR(text, "iBGP");
    CHECK(net_nexam("2017-37", text, (int)sizeof text) == 0);
    CHECK_STR(text, "D");
    CHECK(net_nexam("2023-40", text, (int)sizeof text) == 0);
    CHECK_STR(text, "D");
    CHECK(net_sdn_hop(0, &hop) == 0);
    CHECK_EQ(hop, 2);
    CHECK(net_sdn_hop(1, &hop) == 0);
    CHECK_EQ(hop, 1);
    CHECK(net_dhcp_accept(&state, "REQUEST", &addr) != 0);
    state = 0;
    CHECK(net_dhcp_accept(&state, "DISCOVER", &addr) == 0);
    CHECK(net_dhcp_accept(&state, "OFFER", &addr) == 0);
    CHECK(net_dhcp_accept(&state, "REQUEST", &addr) == 0);
    CHECK(net_dhcp_accept(&state, "ACK", &addr) == 0);
    CHECK(net_ip_fmt(addr, text, (int)sizeof text) == 0);
    CHECK_STR(text, "192.168.1.10");
}

static void test_stack(void) {
    char body[640];
    CHECK(net_stack_line(body, (int)sizeof body) == 0);
    CHECK(strstr(body, "arp target=10.0.0.1") != NULL);
    CHECK(strstr(body, "frag n=2 off=0,97") != NULL);
    CHECK(strstr(body, "nat 4000 4001") != NULL);
    CHECK(strstr(body, "dhcp DISCOVER OFFER REQUEST ACK addr=192.168.1.10") != NULL);
    CHECK(strstr(body, "zero icmp=11 code=0") != NULL);
}

int main(void) {
    test_addr();
    test_agg();
    test_frag_hdr();
    test_fwd();
    test_arp_nat();
    test_route();
    test_opt();
    test_stack();
    return TEST_SUMMARY();
}
