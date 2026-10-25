#include "block.h"
#include "cap.h"
#include "frag.h"
#include "hdr.h"
#include "ip.h"
#include "scan.h"
#include "test.h"

#include <string.h>

static void load_header(const char *hex, IpInfo *ip) {
    const char *p = hex;
    unsigned char buf[IP_CAPTURE_MAX];
    int n = 0;
    char err[80];

    CHECK(parse_hex_bytes(&p, buf, IP_CAPTURE_MAX, &n, err, (int)sizeof err));
    CHECK(ip_parse_header(buf, n, ip, err, (int)sizeof err));
}

static void check_addr(void) {
    uint32_t ip = 0;
    int prefix = 0;

    CHECK(ip_parse("180.80.77.55", &ip));
    CHECK_EQ(ip_network(ip, 22), 0xb4504c00u);
    CHECK_EQ(ip_broadcast(ip, 22), 0xb4504fffu);
    CHECK(ip_parse("180.80.76.0", &ip));
    CHECK_EQ(ip, 0xb4504c00u);
    CHECK_EQ(ip_usable(29), 6);
    CHECK_EQ(ip_usable(25), 126);
    CHECK_EQ(ip_usable(30), 2);
    CHECK_EQ(ip_usable(32), 1);
    CHECK_EQ(ip_usable(31), 0);
    CHECK(mask_to_prefix(0xfffffff8u, &prefix));
    CHECK_EQ(prefix, 29);
    CHECK(!mask_to_prefix(0xffff00ffu, &prefix));
}

static void check_split(void) {
    Block base = {0xc0a80500u, 24};
    Block wide = {0xca760100u, 24};
    Block big = {0x65c81000u, 20};
    EqualSplit equal;
    MaskSplit mask;
    VlsmSplit vlsm;
    char err[80];

    CHECK(subnet_equal(wide, 120, 2, &equal, err, (int)sizeof err));
    CHECK_EQ(equal.item[0].prefix, 25);
    CHECK_EQ(equal.item[1].base, 0xca760180u);
    CHECK_EQ(equal.usable, 126);
    CHECK(subnet_by_mask(base, 29, &mask, err, (int)sizeof err));
    CHECK_EQ(mask.count, 32);
    CHECK_EQ(mask.usable, 6);
    CHECK(split_min(big, 5, &vlsm, err, (int)sizeof err));
    CHECK_EQ(vlsm.item[4].prefix, 24);
    CHECK_EQ(vlsm.item[4].base, 0x65c81f00u);
    CHECK_EQ(ip_usable(vlsm.item[4].prefix), 254);
}

static void check_agg(void) {
    Block four[4] = {{0x23e62000u, 21}, {0x23e62800u, 21}, {0x23e63000u, 21}, {0x23e63800u, 21}};
    Block two[2] = {{0xc0a80200u, 25}, {0xc0a80280u, 25}};
    AggResult agg;
    char err[80];

    CHECK(aggregate_blocks(four, 4, &agg, err, (int)sizeof err));
    CHECK_EQ(agg.out.base, 0x23e62000u);
    CHECK_EQ(agg.out.prefix, 19);
    CHECK(agg.exact);
    CHECK(aggregate_blocks(two, 2, &agg, err, (int)sizeof err));
    CHECK_EQ(agg.out.prefix, 24);
    CHECK_EQ(agg.out.base, 0xc0a80200u);
    CHECK(agg.exact);
}

static void check_sibling(void) {
    Block known = {0xc0a80980u, 26};
    SiblingResult result;
    char err[80];
    Block bad = {0xc0a80900u, 26};
    Block ok = {0xc0a80900u, 25};

    CHECK(sibling_blocks(known, bad, 3, &result, err, (int)sizeof err));
    CHECK(!result.possible);
    CHECK_EQ(result.at_small, 4);
    CHECK(sibling_blocks(known, ok, 3, &result, err, (int)sizeof err));
    CHECK(result.possible);
    CHECK_EQ(result.n_holes, 1);
    CHECK_EQ(result.holes[0].prefix, 26);
    CHECK_EQ(result.holes[0].base, 0xc0a809c0u);
}

static void check_frag(void) {
    FragPlan plan;
    char err[80];

    CHECK(frag_plan(1580, 20, 800, 0, &plan, err, (int)sizeof err));
    CHECK_EQ(plan.count, 3);
    CHECK_EQ(plan.max_data, 776);
    CHECK_EQ(plan.piece[1].total_len, 796);
    CHECK_EQ(plan.piece[1].mf, 1);
    CHECK_EQ(plan.piece[1].offset_units, 97);
    CHECK_EQ(plan.piece[2].data_len, 8);
    CHECK_EQ(plan.piece[2].mf, 0);
    CHECK(frag_plan(1500, 20, 800, 0, &plan, err, (int)sizeof err));
    CHECK_EQ(plan.count, 2);
    CHECK_EQ(plan.piece[1].offset_units, 97);
    CHECK_EQ(plan.piece[1].data_len, 704);
    CHECK(frag_plan(2000, 20, 1500, 1, &plan, err, (int)sizeof err));
    CHECK(plan.df_drop);
}

static void check_capture(void) {
    const char *hex[5] = {
        "45 00 00 30 01 9b 40 00 80 06 1d e8 c0 a8 00 08 d3 44 47 50 "
        "0b d9 13 88 84 6b 41 c5 00 00 00 00 70 02 43 80 5d b0 00 00",
        "45 00 00 30 00 00 40 00 31 06 6e 83 d3 44 47 50 c0 a8 00 08 "
        "13 88 0b d9 e0 59 9f ef 84 6b 41 c6 70 12 16 d0 37 e1 00 00",
        "45 00 00 28 01 9c 40 00 80 06 1d ef c0 a8 00 08 d3 44 47 50 "
        "0b d9 13 88 84 6b 41 c6 e0 59 9f f0 50 10 43 80 2b 32 00 00",
        "45 00 00 38 01 9d 40 00 80 06 1d de c0 a8 00 08 d3 44 47 50 "
        "0b d9 13 88 84 6b 41 c6 e0 59 9f f0 50 18 43 80 e6 55 00 00",
        "45 00 00 28 68 11 40 00 31 06 06 7a d3 44 47 50 c0 a8 00 08 "
        "13 88 0b d9 e0 59 9f f0 84 6b 41 d6 50 10 16 d0 57 d2 00 00",
    };
    const char *origin_hex =
        "45 00 00 28 68 11 40 00 40 06 ec ad d3 44 47 50 ca 76 01 06 "
        "13 88 a1 08 e0 59 9f f0 84 6b 41 d6 50 10 16 d0 b7 d6 00 00";
    IpInfo pkt[5];
    IpInfo origin;
    CapResult result;
    char err[80];
    int i;

    for (i = 0; i < 5; i++) {
        load_header(hex[i], &pkt[i]);
        CHECK(pkt[i].checksum_ok);
    }
    CHECK(pkt[0].has_tcp);
    CHECK_EQ(pkt[0].tcp_flags & 0x12, 0x02);
    CHECK_EQ(pkt[2].tcp_flags, 0x10);
    CHECK_EQ(pkt[2].total_len, 40);
    load_header(origin_hex, &origin);
    CHECK(cap_conclude(pkt, 5, 0xc0a80008u, 0xd3444750u, &origin, &result, err, (int)sizeof err));
    CHECK_EQ(result.n_host, 3);
    CHECK_EQ(result.from_host[0], 1);
    CHECK_EQ(result.from_host[2], 4);
    CHECK_EQ(result.hand[0], 1);
    CHECK_EQ(result.hand[1], 2);
    CHECK_EQ(result.hand[2], 3);
    CHECK_EQ(result.n_pad, 2);
    CHECK_EQ(result.padded[0], 3);
    CHECK_EQ(result.padded[1], 5);
    CHECK_EQ(result.app_bytes, 16);
    CHECK_EQ(result.routers, 15);
    CHECK(result.dst_changed);
}

static void check_class(void) {
    IpKind kind;
    IpScope scope;

    ip_classify(0xc0a80008u, &kind, &scope);
    CHECK_EQ(kind, IP_KIND_C);
    CHECK_EQ(scope, IP_SCOPE_PRIVATE);
    ip_classify(0x08080808u, &kind, &scope);
    CHECK_EQ(kind, IP_KIND_A);
    CHECK_EQ(scope, IP_SCOPE_PUBLIC);
    ip_classify(0xe0000001u, &kind, &scope);
    CHECK_EQ(kind, IP_KIND_D);
}

int main(void) {
    check_addr();
    check_split();
    check_agg();
    check_sibling();
    check_frag();
    check_capture();
    check_class();
    return TEST_SUMMARY();
}
