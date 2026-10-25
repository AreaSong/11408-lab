#include "conn.h"
#include "seqcalc.h"
#include "sum.h"
#include "test.h"
#include "udp.h"

static void check_sum(void) {
    unsigned char pair[4] = {0xb9, 0xb6, 0x65, 0xc5};
    unsigned char one = 0x41;
    unsigned int sum = sum_add_bytes(0, pair, 4);

    CHECK_EQ(sum_fold(sum), 0x1f7c);
    CHECK_EQ(sum_finish(sum, 0), 0xe083);
    CHECK_EQ(sum_add_bytes(0, &one, 1), 0x4100);
}

static void check_udp(void) {
    unsigned char src[4] = {10, 0, 0, 1};
    unsigned char dst[4] = {10, 0, 0, 2};
    unsigned char zeroip[4] = {0, 0, 0, 0};
    unsigned char all[4] = {255, 255, 255, 255};
    unsigned char hi[2] = {'H', 'i'};
    unsigned char odd[1] = {'A'};
    unsigned char zero_sum[2] = {0xff, 0xda};
    unsigned char ff[2] = {0xff, 0xff};
    unsigned int tenths = 0;
    UdpGram g;

    CHECK(udp_set(&g, src, dst, 1000, 2000, hi, 2) == NULL);
    CHECK_EQ(g.checksum, 0x97b6);
    CHECK_EQ(g.length, 10);
    CHECK_EQ(udp_verify(&g), UDP_OK);
    g.data[1] ^= 1;
    CHECK_EQ(udp_verify(&g), UDP_BAD);
    g.data[1] ^= 1;
    g.checksum = 0;
    CHECK_EQ(udp_verify(&g), UDP_SKIP);

    CHECK(udp_set(&g, src, dst, 1000, 2000, odd, 1) == NULL);
    CHECK_EQ(g.length, 9);
    CHECK_EQ(g.checksum, 0x9f21);

    CHECK(udp_set(&g, zeroip, zeroip, 0, 0, zero_sum, 2) == NULL);
    CHECK_EQ(g.checksum, 0xffff);
    CHECK_EQ(udp_verify(&g), UDP_OK);

    CHECK(udp_set(&g, all, all, 65535, 65535, ff, 2) == NULL);
    CHECK_EQ(g.checksum, 0xffda);

    CHECK(udp_eff_tenths(12, 0, &tenths));
    CHECK_EQ(tenths, 600);
    CHECK(udp_eff_tenths(12, 1, &tenths));
    CHECK_EQ(tenths, 375);
    CHECK_STR(udp_port_name(69), "TFTP");
}

static void check_seq(void) {
    unsigned int two[2] = {300, 500};
    unsigned int three[3] = {300, 400, 500};
    unsigned int out_seq = 0;
    unsigned int out_ack = 0;
    unsigned int bytes = 0;
    Segs segs;

    CHECK(segs_place(&segs, 2, two, 0, 200) == NULL);
    CHECK_EQ(segs_ack(&segs), 1000);
    CHECK(segs_place(&segs, 3, three, 2, 900) == NULL);
    CHECK_EQ(segs.start[0], 200);
    CHECK_EQ(segs.start[1], 500);
    CHECK(segs_miss(&segs, 1) == NULL);
    CHECK_EQ(segs_hole(&segs), 1);
    CHECK_EQ(segs_ack(&segs), 500);
    CHECK(seq_reply(1913, 2046, 100, &out_seq, &out_ack) == NULL);
    CHECK_EQ(out_seq, 2046);
    CHECK_EQ(out_ack, 2013);
    CHECK(seq_app(1000, 5001, &bytes) == NULL);
    CHECK_EQ(bytes, 4000);
    CHECK(seq_app(1000, 1001, &bytes) == NULL);
    CHECK_EQ(bytes, 0);
    CHECK(seq_app(1000, 1000, &bytes) != NULL);
}

static void check_handshake(void) {
    TcpLink link;
    TcpTrace tr;

    tcp_link_init(&link);
    CHECK(tcp_open(&link, 2018, 2046, 0, &tr) == NULL);
    CHECK_EQ(tr.steps[1].ack, 2019);
    CHECK_EQ(tr.steps[2].ack, 2047);
    CHECK_EQ(tr.steps[2].seq, 2019);
    CHECK_EQ(link.end[TCP_CLIENT].next_seq, 2019);

    tcp_link_init(&link);
    CHECK(tcp_open(&link, 11220, 11221, 0, &tr) == NULL);
    CHECK_EQ(tr.steps[1].syn, 1);
    CHECK_EQ(tr.steps[1].ack_flag, 1);
    CHECK_EQ(tr.steps[1].seq, 11221);
    CHECK_EQ(tr.steps[1].ack, 11221);

    tcp_link_init(&link);
    CHECK(tcp_open(&link, 1000, 5000, 200, &tr) == NULL);
    CHECK_EQ(tr.steps[2].data_len, 200);
    CHECK_EQ(tr.steps[2].seq, 1001);
    CHECK_EQ(tr.steps[2].ack, 5001);
    CHECK_EQ(link.end[TCP_CLIENT].next_seq, 1201);
    CHECK_EQ(link.end[TCP_SERVER].next_ack, 1201);
}

static void check_close(void) {
    TcpLink link;
    TcpTrace tr;

    tcp_link_init(&link);
    CHECK(tcp_open(&link, 1000, 9, 0, &tr) == NULL);
    CHECK(tcp_send(&link, TCP_CLIENT, 4000, &tr) == NULL);
    CHECK_EQ(link.end[TCP_CLIENT].next_seq, 5001);
    CHECK_EQ(tr.steps[1].ack, 5001);
    CHECK(tcp_close(&link, TCP_CLIENT, &tr) == NULL);
    CHECK_EQ(tr.steps[0].seq, 5001);
    CHECK_EQ(tr.steps[0].client_state, ST_FIN_WAIT_1);
    CHECK_EQ(tr.steps[0].server_state, ST_CLOSE_WAIT);
    CHECK_EQ(tr.steps[1].client_state, ST_FIN_WAIT_2);
    CHECK_EQ(tr.steps[2].client_state, ST_TIME_WAIT);
    CHECK_EQ(tr.steps[2].server_state, ST_LAST_ACK);
    CHECK_EQ(tr.steps[3].client_state, ST_TIME_WAIT);
    CHECK_EQ(tr.steps[3].server_state, ST_CLOSED);

    tcp_link_init(&link);
    CHECK(tcp_open(&link, 1, 2, 0, &tr) == NULL);
    CHECK(tcp_close(&link, TCP_SERVER, &tr) == NULL);
    CHECK_EQ(tr.steps[3].client_state, ST_CLOSED);
    CHECK_EQ(tr.steps[3].server_state, ST_TIME_WAIT);

    tcp_link_init(&link);
    CHECK(tcp_open(&link, 7, 9, 0, &tr) == NULL);
    CHECK(tcp_close(&link, TCP_BOTH, &tr) == NULL);
    CHECK_EQ(tr.steps[2].client_state, ST_CLOSING);
    CHECK_EQ(tr.steps[3].server_state, ST_CLOSING);
    CHECK_EQ(tr.steps[5].client_state, ST_TIME_WAIT);
    CHECK_EQ(tr.steps[5].server_state, ST_TIME_WAIT);
}

int main(void) {
    check_sum();
    check_udp();
    check_seq();
    check_handshake();
    check_close();
    return TEST_SUMMARY();
}
