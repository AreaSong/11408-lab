#include "common/mm_machine.h"
#include "cpu/cpu_intr.h"
#include "net/net_bus.h"
#include "net/net_delay.h"
#include "net/net_exam.h"
#include "net/net_fmt.h"
#include "net/net_frame.h"
#include "net/net_mac.h"
#include "net/net_nic.h"
#include "net/net_phy.h"
#include "net/net_slide.h"
#include "net/net_stp.h"
#include "net/net_sw.h"
#include "net/net_win.h"
#include "test.h"

#include <string.h>

static void test_delay(void) {
    NetDelay d;
    int pct = 0;
    long long good = 0;
    long long eff = 0;
    char text[32];
    CHECK(net_link(8000, 10000000LL, 1000000LL, 200000000LL, 0, 0, &d) == 0);
    CHECK_EQ(d.tx_ns, 800000);
    CHECK_EQ(d.prop_ns, 5000000);
    CHECK_EQ(d.total_ns, 5800000);
    CHECK_EQ(d.rtt_ns, 10000000);
    CHECK_EQ(d.bdp_bit, 50000);
    net_fmt_ns(text, (int)sizeof text, d.tx_ns);
    CHECK_STR(text, "0.80 ms");
    CHECK(net_util_pct(8000, 8000, 10000LL, 200000000LL, &pct) == 0);
    CHECK_EQ(pct, 40);
    CHECK(net_goodput(98, 2, 100000000LL, &good) == 0);
    CHECK_EQ(good, 98000000LL);
    CHECK(net_eff(12000, 12144, 512, 10000000LL, 10000, &eff) == 0);
    CHECK_EQ(eff, 9334163LL);
    net_fmt_bps(text, (int)sizeof text, eff);
    CHECK_STR(text, "约 9.33 Mbps");
}

static void test_window(void) {
    int window = 0;
    int bits = 0;
    int send = 0;
    int recv = 0;
    long long rate = 0;
    char rel[32];
    CHECK(net_need("gbn", 1024, 4096, 1, 16000LL, 270000000LL, 100, &window, &bits) == 0);
    CHECK_EQ(window, 11);
    CHECK_EQ(bits, 4);
    CHECK(net_need("slide", 8000, 8000, 0, 128000LL, 250000000LL, 80, &window, &bits) == 0);
    CHECK_EQ(window, 8);
    CHECK_EQ(bits, 4);
    CHECK(net_rate("gbn", 1000, 8000, 0, 100000000LL, 50000000LL, &rate) == 0);
    CHECK_EQ(rate, 79936051LL);
    CHECK(net_rate("stop", 1, 1000, 0, 10000LL, 200000000LL, &rate) == 0);
    CHECK_EQ(rate, 2000);
    CHECK(net_room(3, 5, &recv) == 0);
    CHECK_EQ(recv, 3);
    CHECK(net_limit_eq("gbn", 3, &send, &recv) == 0);
    CHECK_EQ(send, 7);
    CHECK_EQ(recv, 1);
    CHECK(net_limit_eq("sr", 3, &send, &recv) == 0);
    CHECK_EQ(send, 4);
    CHECK_EQ(recv, 4);
    CHECK(net_cmp_u(3, rel, (int)sizeof rel) == 0);
    CHECK_STR(rel, "U1 <= U3 <= U2");
}

static void test_slide(void) {
    int list[8];
    char note[32];
    int front = 0;
    int rear = 0;
    int t1 = 0;
    int t2 = 0;
    int n = net_rexmit("2009-35", list, 8, note, (int)sizeof note);
    CHECK_EQ(n, 4);
    CHECK_EQ(list[0], 4);
    CHECK_EQ(list[3], 7);
    n = net_rexmit("2011-35", list, 8, note, (int)sizeof note);
    CHECK_EQ(n, 2);
    CHECK_EQ(list[0], 0);
    CHECK_EQ(list[1], 2);
    n = net_rexmit("practice-gbn", list, 8, note, (int)sizeof note);
    CHECK_EQ(n, 3);
    CHECK_EQ(list[0], 2);
    CHECK_EQ(list[2], 4);
    CHECK(net_sr_2024(&front, &rear, &t1, &t2) > 0);
    CHECK_EQ(front, 1);
    CHECK_EQ(rear, 0);
    CHECK_EQ(t1, 4);
    CHECK_EQ(t2, 1);
    n = net_rexmit("2024-37", list, 8, note, (int)sizeof note);
    CHECK_EQ(n, 1);
    CHECK_STR(note, "t1=F4 t2=F1");
}

static void test_frame(void) {
    char out[128];
    char fixed[80];
    char raw[32];
    char rem[16];
    int clean = 0;
    int bad = 0;
    int frame = 0;
    int pad = 0;
    CHECK(net_bit_stuff("0111110001111110", out, (int)sizeof out) == 0);
    CHECK_STR(out, "011111000011111010");
    CHECK(net_byte_stuff("7E 21 7D 01 41", out, (int)sizeof out) == 0);
    CHECK_STR(out, "7D 5E 21 7D 5D 7D 21 41");
    CHECK(net_crc_rem("101111100", "10011", rem, (int)sizeof rem, &clean) == 0);
    CHECK_EQ(clean, 1);
    CHECK_STR(rem, "0000");
    CHECK(net_crc_rem("101110000", "10011", rem, (int)sizeof rem, &clean) == 0);
    CHECK_EQ(clean, 0);
    CHECK_STR(rem, "1100");
    CHECK(net_crc_rem("101001001", "1101", rem, (int)sizeof rem, &clean) == 0);
    CHECK_EQ(clean, 1);
    CHECK_EQ(net_ham_need(8), 4);
    CHECK_EQ(net_ham_need(4), 3);
    CHECK(net_ham_flip("1010", 5, &bad, fixed, (int)sizeof fixed, raw, (int)sizeof raw) == 0);
    CHECK_EQ(bad, 5);
    CHECK_STR(raw, "1010");
    CHECK_STR(fixed, "1011010");
    CHECK(net_eth_pad(2, 0, &frame, &pad) == 0);
    CHECK_EQ(frame, 64);
    CHECK_EQ(pad, 44);
    CHECK(net_eth_pad(2, 1, &frame, &pad) == 0);
    CHECK_EQ(frame, 68);
    CHECK_EQ(pad, 44);
    CHECK_EQ(net_ber_flips(1, 0, 8, 4), 0);
    CHECK_EQ(net_ber_flips(1, 1000000, 8, 1), 8);
}

static void test_mac(void) {
    char got[16];
    char pct[32];
    long long useful = 0;
    long long total = 0;
    CHECK_EQ(net_delta_m(800, 1000000000LL, 200000000LL), 80);
    CHECK_EQ(net_reach_m(512, 100000000LL, 1535, 200), 205);
    CHECK_EQ(net_one_way_ns(1024, 100000000LL), 5120);
    CHECK_EQ(net_min_bits(10000000LL, 2500, 200000000LL), 250);
    CHECK_EQ(net_backoff_ns(4, 51200), 768000);
    CHECK_EQ(net_backoff_ns(16, 51200), -1);
    CHECK(net_cdma_2014(got, (int)sizeof got) == 0);
    CHECK_STR(got, "101");
    CHECK(net_bus_csma(2, 1, 4, &useful, &total) == 0);
    CHECK_EQ(useful, 8);
    CHECK_EQ(total, 9);
    CHECK(net_bus_csma(2, 2, 8, &useful, &total) == 0);
    CHECK_EQ(useful, 32);
    CHECK_EQ(total, 34);
    net_fmt_pct(pct, (int)sizeof pct, useful, total);
    CHECK_STR(pct, "94.12%");
    CHECK(net_bus_token(2, 2, 8, 1, &useful, &total) == 0);
    CHECK_EQ(useful, 32);
    CHECK_EQ(total, 35);
    CHECK(net_bus_csma(20, 2, 8, &useful, &total) == 0);
    CHECK_EQ(useful, 320);
    CHECK_EQ(total, 831);
    CHECK(net_bus_token(20, 2, 8, 1, &useful, &total) == 0);
    CHECK_EQ(useful, 320);
    CHECK_EQ(total, 359);
}

static void test_switch(void) {
    char a[64];
    char b[64];
    char miss = 0;
    int ports[8];
    int n = 0;
    int col = 0;
    int brd = 0;
    long long bps = 0;
    int approx = 0;
    CHECK(net_sw_2014(a, (int)sizeof a, b, (int)sizeof b) == 0);
    CHECK_STR(a, "fwd=2,3 got=b1,c1");
    CHECK_STR(b, "fwd=1 got=a1");
    CHECK(net_sw_2016(a, (int)sizeof a, b, (int)sizeof b) == 0);
    CHECK_STR(a, "fwd=2,3 got=H1,H3,H4");
    CHECK_STR(b, "fwd=1 got=H2,H3");
    CHECK(net_domains(4, 1, &col, &brd) == 0);
    CHECK_EQ(col, 4);
    CHECK_EQ(brd, 2);
    CHECK(net_place("switch", 4, &col, &brd) == 0);
    CHECK_EQ(col, 4);
    CHECK_EQ(brd, 1);
    CHECK(net_vlan_2024(&miss, ports, 8, &n) == 0);
    CHECK_EQ(miss, 'D');
    CHECK(net_wifi_exam("2017-35", a, (int)sizeof a) == 0);
    CHECK_STR(a, "00-12-34-56-78-9b,00-12-34-56-78-9a,00-12-34-56-78-9c");
    CHECK(net_wifi_exam("2022-47", a, (int)sizeof a) == 0);
    CHECK_STR(a, "00-11-11-11-11-e1,00-11-11-11-11-c1,00-11-11-11-11-d1");
    CHECK_EQ(net_cut_ns(6, 100), 480);
    CHECK_EQ(net_cut_ns(64, 100), 5120);
    CHECK_EQ(net_cut_ns(1518, 100), 121440);
    CHECK(net_stp_demo("kruskal", a, (int)sizeof a) == 0);
    CHECK_STR(a, "keep=1-2,2-3 drop=1-3");
    CHECK(net_stp_demo("prim", a, (int)sizeof a) == 0);
    CHECK_STR(a, "order=1-2,2-3 drop=1-3");
    CHECK(net_flood_demo(a, (int)sizeof a) == 0);
    CHECK_STR(a, "0,1,2,3");
    CHECK(net_nyquist(3000, 16, &bps) == 0);
    CHECK_EQ(bps, 24000);
    CHECK(net_shannon(8000, 30, 50, &bps, &approx) == 0);
    CHECK_EQ(bps, 40000);
    CHECK_EQ(approx, 1);
    CHECK(net_qam(4000000LL, 48000000LL, a, (int)sizeof a) == 0);
    CHECK_STR(a, "QAM-64");
    CHECK_EQ(net_min_states(30), 32);
    CHECK(net_manch("0110", a, (int)sizeof a) == 0);
    CHECK_STR(a, "01101001");
}

static void test_letters(void) {
    char letter[4];
    CHECK(net_lexam("2009-35", letter, 4) == 0);
    CHECK_STR(letter, "C");
    CHECK(net_lexam("2014-36", letter, 4) == 0);
    CHECK_STR(letter, "C");
    CHECK(net_lexam("2014-37", letter, 4) == 0);
    CHECK_STR(letter, "B");
    CHECK(net_lexam("2024-37", letter, 4) == 0);
    CHECK_STR(letter, "D");
    CHECK(net_lexam("2016-35", letter, 4) == 0);
    CHECK_STR(letter, "D");
    CHECK(net_lexam("2020-36", letter, 4) == 0);
    CHECK_STR(letter, "D");
}

static void test_nic(void) {
    MmMachine *m = mm_create(65536);
    unsigned char raw[4] = {0x11, 0x22, 0x33, 0x44};
    CHECK(m != NULL);
    CHECK(net_on(m) == 0);
    CHECK(net_put(m, 4096, raw, 4) == 0);
    CHECK(net_arm_tx(m, 4096, 4) == 0);
    CHECK(net_pump(m) == 0);
    CHECK_EQ(cpu_irq_pending(m, 2), 1);
    CHECK_EQ(net_stat(m), 6);
    net_after_reset(m);
    CHECK(net_put(m, 4096, raw, 4) == 0);
    CHECK(net_corrupt(m, 1) == 0);
    CHECK(net_arm_tx(m, 4096, 4) == 0);
    CHECK(net_pump(m) == 0);
    CHECK_EQ(net_stat(m), 10);
    mm_destroy(m);
}

int main(void) {
    test_delay();
    test_window();
    test_slide();
    test_frame();
    test_mac();
    test_switch();
    test_letters();
    test_nic();
    return TEST_SUMMARY();
}
