#include "bgp.h"
#include "bytes.h"
#include "checksum.h"
#include "mcast.h"
#include "mip.h"
#include "test.h"

#include <string.h>

static void check_prefix(void) {
    char out[32];
    char err[80];

    CHECK(bgp_prefix_ok("10.0.0.0/8", out, (int)sizeof out, err, (int)sizeof err));
    CHECK_STR(out, "10.0.0.0/8");
    CHECK(bgp_prefix_ok("136.5.16.0/20", out, (int)sizeof out, err, (int)sizeof err));
    CHECK(!bgp_prefix_ok("10.0.0.1/8", out, (int)sizeof out, err, (int)sizeof err));
    CHECK(!bgp_prefix_ok("10.0.0.0", out, (int)sizeof out, err, (int)sizeof err));
}

static void check_loop(void) {
    BgpNet net;
    int path[4];
    int local = 0;
    char err[80];
    char prefix[20];

    bgp_init(&net);
    CHECK(bgp_add_as(&net, 1, err, (int)sizeof err));
    CHECK(bgp_add_as(&net, 2, err, (int)sizeof err));
    CHECK(bgp_add_as(&net, 3, err, (int)sizeof err));
    CHECK(bgp_add_peer(&net, 1, 2, err, (int)sizeof err));
    CHECK(bgp_add_peer(&net, 2, 3, err, (int)sizeof err));
    CHECK(bgp_add_peer(&net, 3, 1, err, (int)sizeof err));
    CHECK(bgp_prefix_ok("10.0.0.0/8", prefix, (int)sizeof prefix, err, (int)sizeof err));
    CHECK(bgp_add_origin(&net, 1, prefix, err, (int)sizeof err));
    CHECK_EQ(bgp_step(&net, NULL), 2);
    CHECK_EQ(bgp_path(&net, 2, prefix, path, 4, &local), 1);
    CHECK_EQ(local, 0);
    CHECK_EQ(path[0], 1);
    CHECK_EQ(bgp_path(&net, 3, prefix, path, 4, &local), 1);
    CHECK_EQ(path[0], 1);
    CHECK_EQ(bgp_path(&net, 1, prefix, path, 4, &local), 0);
    CHECK_EQ(local, 1);
    CHECK_EQ(bgp_step(&net, NULL), 0);
    CHECK_EQ(bgp_path(&net, 1, prefix, path, 4, &local), 0);
    CHECK_EQ(local, 1);
}

static void check_shorter(void) {
    BgpNet net;
    int path[4];
    int local = 1;
    char err[80];
    char prefix[20];

    bgp_init(&net);
    CHECK(bgp_add_as(&net, 1, err, (int)sizeof err));
    CHECK(bgp_add_as(&net, 2, err, (int)sizeof err));
    CHECK(bgp_add_as(&net, 3, err, (int)sizeof err));
    CHECK(bgp_add_peer(&net, 1, 2, err, (int)sizeof err));
    CHECK(bgp_add_peer(&net, 2, 3, err, (int)sizeof err));
    CHECK(bgp_prefix_ok("10.1.0.0/16", prefix, (int)sizeof prefix, err, (int)sizeof err));
    CHECK(bgp_add_origin(&net, 1, prefix, err, (int)sizeof err));
    CHECK_EQ(bgp_step(&net, NULL), 1);
    CHECK_EQ(bgp_path(&net, 3, prefix, path, 4, &local), -1);
    CHECK_EQ(bgp_step(&net, NULL), 1);
    CHECK_EQ(bgp_path(&net, 3, prefix, path, 4, &local), 2);
    CHECK_EQ(path[0], 2);
    CHECK_EQ(path[1], 1);
    CHECK(bgp_add_peer(&net, 1, 3, err, (int)sizeof err));
    CHECK_EQ(bgp_step(&net, NULL), 1);
    CHECK_EQ(bgp_path(&net, 3, prefix, path, 4, &local), 1);
    CHECK_EQ(path[0], 1);
    CHECK_EQ(local, 0);
}

static void offer4(BgpNet *net, const char *who, const char *via, int igp, const int *path, int npath) {
    char err[80];
    char prefix[20];

    CHECK(bgp_prefix_ok("136.5.16.0/20", prefix, (int)sizeof prefix, err, (int)sizeof err));
    CHECK(bgp_add_offer(net, who, prefix, via, igp, path, npath, err, (int)sizeof err));
}

static void check_pick(void) {
    BgpNet net;
    char via[8];
    char err[80];
    int p3[3];
    int p4[4];

    bgp_init(&net);
    p3[0] = 2;
    p3[1] = 8;
    p3[2] = 19;
    p4[0] = 3;
    p4[1] = 7;
    p4[2] = 11;
    p4[3] = 19;
    offer4(&net, "R14", "R11", 1, p3, 3);
    offer4(&net, "R14", "R12", 2, p4, 4);
    p3[0] = 4;
    p3[1] = 10;
    p3[2] = 19;
    offer4(&net, "R14", "R13", 2, p3, 3);
    CHECK(bgp_pick(&net, "R14", "136.5.16.0/20", via, (int)sizeof via, NULL, err, (int)sizeof err));
    CHECK_STR(via, "R11");

    p3[0] = 2;
    p3[1] = 8;
    p3[2] = 19;
    offer4(&net, "R15", "R11", 2, p3, 3);
    offer4(&net, "R15", "R12", 2, p4, 4);
    p3[0] = 4;
    p3[1] = 10;
    p3[2] = 19;
    offer4(&net, "R15", "R13", 1, p3, 3);
    CHECK(bgp_pick(&net, "R15", "136.5.16.0/20", via, (int)sizeof via, NULL, err, (int)sizeof err));
    CHECK_STR(via, "R13");
}

static int collide(const unsigned char ip[4]) {
    unsigned char base[6];
    unsigned char mac[6];
    unsigned char trial[4];
    int a;
    int bit;
    int n = 0;

    CHECK(mcast_map(ip, base));
    trial[2] = ip[2];
    trial[3] = ip[3];
    for (a = 224; a <= 239; a++) {
        for (bit = 0; bit < 2; bit++) {
            trial[0] = (unsigned char)a;
            trial[1] = (unsigned char)((ip[1] & 0x7f) | (bit << 7));
            CHECK(mcast_map(trial, mac));
            if (memcmp(mac, base, 6) == 0) {
                n++;
            }
        }
    }
    return n;
}

static void check_mcast(void) {
    unsigned char ip[4];
    unsigned char mac[6];
    unsigned char other[6];

    CHECK(parse_ipv4("224.0.1.1", ip));
    CHECK(mcast_map(ip, mac));
    CHECK_EQ(mac[0], 0x01);
    CHECK_EQ(mac[1], 0x00);
    CHECK_EQ(mac[2], 0x5e);
    CHECK_EQ(mac[3], 0x00);
    CHECK_EQ(mac[4], 0x01);
    CHECK_EQ(mac[5], 0x01);
    CHECK(parse_ipv4("224.128.1.1", ip));
    CHECK(mcast_map(ip, other));
    CHECK(memcmp(mac, other, 6) == 0);
    CHECK(parse_ipv4("225.0.1.1", ip));
    CHECK(mcast_map(ip, other));
    CHECK(memcmp(mac, other, 6) == 0);
    CHECK(parse_ipv4("224.0.1.2", ip));
    CHECK(mcast_map(ip, other));
    CHECK(memcmp(mac, other, 6) != 0);
    CHECK_EQ(other[5], 0x02);
    CHECK(parse_ipv4("224.215.145.230", ip));
    CHECK(mcast_map(ip, mac));
    CHECK_EQ(mac[3], 0x57);
    CHECK_EQ(mac[4], 0x91);
    CHECK_EQ(mac[5], 0xe6);
    CHECK(parse_ipv4("224.0.0.5", ip));
    CHECK(mcast_map(ip, mac));
    CHECK_EQ(mac[5], 0x05);
    CHECK(parse_ipv4("192.168.1.1", ip));
    CHECK(!mcast_map(ip, mac));
    CHECK(parse_ipv4("224.0.1.1", ip));
    CHECK_EQ(collide(ip), 32);
    CHECK_EQ(mcast_same_count(), 32);
}

static void check_tunnel(void) {
    unsigned char cn[4];
    unsigned char home[4];
    unsigned char ha[4];
    unsigned char coa[4];
    unsigned char inner[128];
    unsigned char saved[128];
    unsigned char outer[256];
    char err[80];
    int inner_len = 0;
    int outer_len = 0;
    unsigned short sum;

    CHECK(parse_ipv4("20.0.0.2", cn));
    CHECK(parse_ipv4("10.0.0.8", home));
    CHECK(parse_ipv4("10.0.0.1", ha));
    CHECK(parse_ipv4("30.0.0.1", coa));
    CHECK(mip_datagram(cn, home, (const unsigned char *)"hi", 2, inner, (int)sizeof inner, &inner_len,
                       err, (int)sizeof err));
    CHECK_EQ(inner_len, 42);
    CHECK_EQ(inner[8], 64);
    CHECK_EQ(inner[9], 6);
    memcpy(saved, inner, (size_t)inner_len);
    CHECK(mip_wrap(inner, inner_len, ha, coa, 64, outer, (int)sizeof outer, &outer_len, err,
                   (int)sizeof err));
    CHECK_EQ(outer_len, 62);
    CHECK_EQ(outer[0], 0x45);
    CHECK_EQ(outer[9], 4);
    CHECK_EQ(outer[8], 64);
    CHECK(memcmp(outer + 12, ha, 4) == 0);
    CHECK(memcmp(outer + 16, coa, 4) == 0);
    CHECK(memcmp(outer + 20, saved, (size_t)inner_len) == 0);
    sum = ip_header_checksum(outer, 20);
    CHECK_EQ(read_be16(outer + 10), sum);
    CHECK(mip_hop(outer, outer_len, err, (int)sizeof err));
    CHECK_EQ(outer[8], 63);
    CHECK_EQ(outer[20 + 8], 64);
    CHECK(memcmp(outer + 20, saved, (size_t)inner_len) == 0);
    sum = ip_header_checksum(outer, 20);
    CHECK_EQ(read_be16(outer + 10), sum);
}

int main(void) {
    check_prefix();
    check_loop();
    check_shorter();
    check_pick();
    check_mcast();
    check_tunnel();
    return TEST_SUMMARY();
}
