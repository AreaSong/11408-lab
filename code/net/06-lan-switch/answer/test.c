#include "ethframe.h"
#include "fwd.h"
#include "fwdlay.h"
#include "mactab.h"
#include "qty.h"
#include "test.h"
#include "topo.h"
#include "wifi11.h"

#include <stdio.h>
#include <string.h>

static void must_mac(const char *text, unsigned char mac[MAC_LEN]) {
    char err[LAB_ERR];
    CHECK(mac_parse(text, mac, err));
}

static int recv_named(const Topo *t, const FwdTrace *tr, const char *name) {
    int i;
    int id = topo_find(t, name);

    for (i = 0; i < tr->nrecv; i++) {
        if (tr->recv[i] == id) {
            return 1;
        }
    }
    return 0;
}

static void test_eth(void) {
    unsigned char hi[2] = {0x68, 0x69};
    unsigned char five[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    EthSpec spec;
    EthFrame fr;
    char err[LAB_ERR];

    memset(&spec, 0, sizeof spec);
    must_mac("ff-ff-ff-ff-ff-ff", spec.dst);
    must_mac("00-11-22-33-44-55", spec.src);
    spec.field = 0x0800;
    spec.data = hi;
    spec.data_len = 2;
    CHECK(eth_build(&spec, &fr, err));
    CHECK_EQ(fr.raw_len, 64);
    CHECK_EQ(fr.pad, 44);
    CHECK_EQ(fr.fcs, 0x892c3ae8u);
    CHECK_EQ(fr.raw[60], 0xe8);
    CHECK_EQ(fr.raw[63], 0x89);

    spec.vlan = 10;
    CHECK(eth_build(&spec, &fr, err));
    CHECK_EQ(fr.raw_len, 68);
    CHECK_EQ(fr.raw[12], 0x81);
    CHECK_EQ(fr.raw[13], 0x00);
    CHECK_EQ(fr.raw[15], 10);
    CHECK_EQ(fr.fcs, 0x9ad2675fu);

    spec.vlan = 0;
    spec.is_len = 1;
    spec.field = 5;
    spec.data = five;
    spec.data_len = 5;
    CHECK(eth_build(&spec, &fr, err));
    CHECK_EQ(fr.pad, 41);
    CHECK_EQ(fr.raw[12], 0x00);
    CHECK_EQ(fr.raw[13], 0x05);
    CHECK_EQ(fr.fcs, 0x08fa1142u);
    spec.data_len = 1501;
    CHECK(!eth_build(&spec, &fr, err));
}

static void test_table(void) {
    MacTab tab;
    unsigned char mac[MAC_LEN];
    int port = 0;
    int old = 0;
    int i;

    mactab_init(&tab);
    memset(mac, 0, sizeof mac);
    mac[5] = 3;
    CHECK_EQ(mactab_learn(&tab, mac, 1, 4, &old), 1);
    CHECK_EQ(mactab_learn(&tab, mac, 2, 9, &old), 1);
    CHECK(mactab_find(&tab, mac, 1, &port));
    CHECK_EQ(port, 4);
    CHECK(!mactab_find(&tab, mac, 3, &port));
    CHECK_EQ(mactab_learn(&tab, mac, 1, 9, &old), 3);
    CHECK_EQ(old, 4);
    for (i = 0; i < MACTAB_CAP - 2; i++) {
        mac[5] = (unsigned char)(10 + i);
        CHECK_EQ(mactab_learn(&tab, mac, 1, 1, &old), 1);
    }
    mac[5] = 200;
    CHECK_EQ(mactab_learn(&tab, mac, 1, 1, &old), 0);
}

static int add_host(Topo *t, const char *name, const char *text) {
    unsigned char mac[MAC_LEN];
    char err[LAB_ERR];

    must_mac(text, mac);
    return topo_add_host(t, name, mac, NULL, err);
}

static void test_2014(void) {
    Topo t;
    FwdTrace tr;
    unsigned char dst[MAC_LEN];
    char err[LAB_ERR];
    int ports[2] = {2, 3};

    topo_init(&t);
    CHECK(topo_add(&t, DEV_SWITCH, "SW", err));
    CHECK(add_host(&t, "a1", "00-e1-d5-00-23-a1"));
    CHECK(add_host(&t, "b1", "00-e1-d5-00-23-b1"));
    CHECK(add_host(&t, "c1", "00-e1-d5-00-23-c1"));
    CHECK(topo_link(&t, "a1", "SW", -1, err));
    CHECK(topo_link(&t, "b1", "SW", -1, err));
    CHECK(topo_link(&t, "c1", "SW", -1, err));
    must_mac("00-e1-d5-00-23-c1", dst);
    CHECK(fwd_send(&t, topo_find(&t, "a1"), dst, &tr));
    CHECK_EQ(tr.nhop, 2);
    CHECK_EQ(tr.hop_port[0], ports[0]);
    CHECK_EQ(tr.hop_port[1], ports[1]);
    CHECK(recv_named(&t, &tr, "b1"));
    CHECK(recv_named(&t, &tr, "c1"));
    CHECK(!recv_named(&t, &tr, "a1"));
    must_mac("00-e1-d5-00-23-a1", dst);
    CHECK(fwd_send(&t, topo_find(&t, "c1"), dst, &tr));
    CHECK_EQ(tr.nhop, 1);
    CHECK_EQ(tr.hop_port[0], 1);
    CHECK(recv_named(&t, &tr, "a1"));
    CHECK(!recv_named(&t, &tr, "b1"));
}

static void test_2016(void) {
    Topo t;
    FwdTrace tr;
    unsigned char dst[MAC_LEN];
    char err[LAB_ERR];

    topo_init(&t);
    CHECK(topo_add(&t, DEV_SWITCH, "SW", err));
    CHECK(topo_add(&t, DEV_HUB, "Hub", err));
    CHECK(add_host(&t, "H2", "00-00-00-00-00-22"));
    CHECK(add_host(&t, "H1", "00-00-00-00-00-11"));
    CHECK(add_host(&t, "H3", "00-00-00-00-00-33"));
    CHECK(add_host(&t, "H4", "00-00-00-00-00-44"));
    CHECK(topo_link(&t, "H2", "SW", -1, err));
    CHECK(topo_link(&t, "H1", "SW", -1, err));
    CHECK(topo_link(&t, "Hub", "SW", -1, err));
    CHECK(topo_link(&t, "H3", "Hub", -1, err));
    CHECK(topo_link(&t, "H4", "Hub", -1, err));
    must_mac("00-00-00-00-00-44", dst);
    CHECK(fwd_send(&t, topo_find(&t, "H2"), dst, &tr));
    must_mac("00-00-00-00-00-22", dst);
    CHECK(fwd_send(&t, topo_find(&t, "H4"), dst, &tr));
    CHECK_EQ(tr.nhop, 1);
    CHECK_EQ(tr.hop_port[0], 1);
    CHECK(recv_named(&t, &tr, "H2"));
    CHECK(recv_named(&t, &tr, "H3"));
    CHECK(!recv_named(&t, &tr, "H1"));
    CHECK(!recv_named(&t, &tr, "H4"));
}

static void test_filter_and_vlan(void) {
    Topo t;
    FwdTrace tr;
    DomainReport rep;
    unsigned char dst[MAC_LEN];
    char err[LAB_ERR];

    topo_init(&t);
    CHECK(topo_add(&t, DEV_SWITCH, "SW", err));
    CHECK(topo_add(&t, DEV_HUB, "Hub", err));
    CHECK(add_host(&t, "a", "02-00-00-00-00-0a"));
    CHECK(add_host(&t, "b", "02-00-00-00-00-0b"));
    CHECK(add_host(&t, "c", "02-00-00-00-00-0c"));
    CHECK(topo_link(&t, "Hub", "SW", -1, err));
    CHECK(topo_link(&t, "c", "SW", -1, err));
    CHECK(topo_link(&t, "a", "Hub", -1, err));
    CHECK(topo_link(&t, "b", "Hub", -1, err));
    must_mac("02-00-00-00-00-0b", dst);
    CHECK(fwd_send(&t, topo_find(&t, "a"), dst, &tr));
    must_mac("02-00-00-00-00-0a", dst);
    CHECK(fwd_send(&t, topo_find(&t, "b"), dst, &tr));
    CHECK(tr.did_filter);
    CHECK(recv_named(&t, &tr, "a"));
    CHECK(!recv_named(&t, &tr, "c"));

    topo_init(&t);
    CHECK(topo_add(&t, DEV_SWITCH, "SW", err));
    CHECK(add_host(&t, "H1", "00-3e-c2-39-12-b5"));
    CHECK(add_host(&t, "H4", "02-00-00-00-00-04"));
    CHECK(add_host(&t, "H6", "00-08-6e-05-a7-82"));
    CHECK(topo_link(&t, "H1", "SW", 1, err));
    CHECK(topo_link(&t, "H4", "SW", 1, err));
    CHECK(topo_link(&t, "H6", "SW", 3, err));
    memset(dst, 0xff, sizeof dst);
    CHECK(fwd_send(&t, topo_find(&t, "H4"), dst, &tr));
    CHECK(recv_named(&t, &tr, "H1"));
    CHECK(!recv_named(&t, &tr, "H6"));
    CHECK(topo_domains(&t, &rep, err));
    CHECK_EQ(rep.ncol, 3);
    CHECK_EQ(rep.nbrd, 2);
}

static void test_2020_and_star(void) {
    Topo t;
    DomainReport r;
    char err[LAB_ERR];
    const char *hubs[] = {"HL1", "HL2", "HR1", "HR2"};
    int i;

    topo_init(&t);
    CHECK(topo_add(&t, DEV_ROUTER, "R", err));
    CHECK(topo_add(&t, DEV_SWITCH, "SL", err));
    CHECK(topo_add(&t, DEV_SWITCH, "SR", err));
    for (i = 0; i < 4; i++) {
        char host[8];
        unsigned char mac[MAC_LEN] = {0x02, 0, 0, 0, 0, 0};

        CHECK(topo_add(&t, DEV_HUB, hubs[i], err));
        mac[5] = (unsigned char)(i + 1);
        snprintf(host, sizeof host, "h%d", i + 1);
        CHECK(topo_add_host(&t, host, mac, NULL, err));
        CHECK(topo_link(&t, host, hubs[i], -1, err));
    }
    CHECK(topo_link(&t, "HL1", "SL", -1, err));
    CHECK(topo_link(&t, "HL2", "SL", -1, err));
    CHECK(topo_link(&t, "SL", "R", -1, err));
    CHECK(topo_link(&t, "R", "SR", -1, err));
    CHECK(topo_link(&t, "HR1", "SR", -1, err));
    CHECK(topo_link(&t, "HR2", "SR", -1, err));
    CHECK(topo_domains(&t, &r, err));
    CHECK_EQ(r.ncol, 4);
    CHECK_EQ(r.nbrd, 2);

    CHECK(topo_star(&t, DEV_HUB, 4, err));
    CHECK(topo_domains(&t, &r, err));
    CHECK_EQ(r.ncol, 1);
    CHECK_EQ(r.nbrd, 1);
    CHECK(topo_star(&t, DEV_SWITCH, 4, err));
    CHECK(topo_domains(&t, &r, err));
    CHECK_EQ(r.ncol, 4);
    CHECK_EQ(r.nbrd, 1);
    CHECK(topo_star(&t, DEV_ROUTER, 4, err));
    CHECK(topo_domains(&t, &r, err));
    CHECK_EQ(r.ncol, 4);
    CHECK_EQ(r.nbrd, 4);
}

static void test_wifi_and_delay(void) {
    unsigned char sta[MAC_LEN];
    unsigned char ap[MAC_LEN];
    unsigned char gw[MAC_LEN];
    unsigned char h4[MAC_LEN];
    unsigned char h5[MAC_LEN];
    WifiFrame w;
    LayTime lay;
    long long bps = 0;
    char err[LAB_ERR];

    must_mac("00-12-34-56-78-9a", sta);
    must_mac("00-12-34-56-78-9b", ap);
    must_mac("00-12-34-56-78-9c", gw);
    wifi_sta_ap(&w, sta, ap, gw);
    CHECK_EQ(w.to_ds, 1);
    CHECK_EQ(w.from_ds, 0);
    CHECK(mac_eq(w.addr[0], ap));
    CHECK(mac_eq(w.addr[1], sta));
    CHECK(mac_eq(w.addr[2], gw));

    must_mac("00-11-11-11-11-c1", ap);
    must_mac("00-11-11-11-11-d1", h4);
    must_mac("00-11-11-11-11-e1", h5);
    wifi_ap_sta(&w, ap, h5, h4);
    CHECK_EQ(w.to_ds, 0);
    CHECK_EQ(w.from_ds, 1);
    CHECK(mac_eq(w.addr[0], h5));
    CHECK(mac_eq(w.addr[1], ap));
    CHECK(mac_eq(w.addr[2], h4));

    CHECK(parse_bps(100, "Mbps", &bps));
    CHECK(fwdlay_ns(0, bps, 0, &lay, err));
    CHECK_EQ(lay.bits, 48);
    CHECK_EQ(lay.ns, 480);
    CHECK(fwdlay_ns(1, bps, 64, &lay, err));
    CHECK_EQ(lay.ns, 5120);
    CHECK(fwdlay_ns(1, bps, 1518, &lay, err));
    CHECK_EQ(lay.ns, 121440);
}

int main(void) {
    test_eth();
    test_table();
    test_2014();
    test_2016();
    test_filter_and_vlan();
    test_2020_and_star();
    test_wifi_and_delay();
    return TEST_SUMMARY();
}
