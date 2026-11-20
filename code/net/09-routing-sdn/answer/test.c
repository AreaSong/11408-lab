#include "ls.h"
#include "rip.h"
#include "sdn.h"
#include "test.h"

static char err[96];

static int node(Topo *t, const char *name, int stub) {
    return topo_add_node(t, name, stub, err, (int)sizeof err);
}

static void check_tie(void) {
    Topo t;
    LsRow rows[4];
    int n = 0;

    topo_init(&t);
    CHECK(node(&t, "0", 0) == 0);
    CHECK(node(&t, "1", 0) == 1);
    CHECK(node(&t, "2", 0) == 2);
    CHECK(node(&t, "3", 0) == 3);
    CHECK(topo_link(&t, "0", "1", 1, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "0", "2", 1, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "1", "3", 1, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "2", "3", 1, err, (int)sizeof err) == 0);
    CHECK(ls_rows(&t, 0, 0, rows, 4, &n, err, (int)sizeof err) == 0);
    CHECK_EQ(n, 3);
    CHECK_EQ(rows[2].cost, 2);
    CHECK_EQ(rows[2].next, 1);
    CHECK_STR(rows[2].path, "0>1>3");
}

static void check_2014(void) {
    Topo t;
    LsRow rows[4];
    int n = 0;
    const char *nets[] = {"192.1.1.0/24", "192.1.5.0/24", "192.1.6.0/24", "192.1.7.0/24"};

    topo_init(&t);
    CHECK(node(&t, "R1", 0) >= 0);
    CHECK(node(&t, "R2", 0) >= 0);
    CHECK(node(&t, "R3", 0) >= 0);
    CHECK(node(&t, "R4", 0) >= 0);
    for (int i = 0; i < 4; i++) {
        CHECK(node(&t, nets[i], 1) >= 0);
    }
    CHECK(topo_link(&t, "R1", "R2", 3, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "R1", "R3", 2, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "R2", "R4", 4, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "R3", "R4", 6, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "R1", "192.1.1.0/24", 1, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "R3", "192.1.5.0/24", 1, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "R2", "192.1.6.0/24", 1, err, (int)sizeof err) == 0);
    CHECK(topo_link(&t, "R4", "192.1.7.0/24", 1, err, (int)sizeof err) == 0);
    CHECK(ls_rows(&t, 0, 1, rows, 4, &n, err, (int)sizeof err) == 0);
    CHECK_EQ(n, 4);
    CHECK(rows[0].direct_stub);
    CHECK_EQ(rows[0].cost, 1);
    CHECK_STR(rows[0].path, "直接到达");
    CHECK_EQ(rows[1].cost, 3);
    CHECK_EQ(rows[1].next, topo_find(&t, "R3"));
    CHECK_STR(rows[1].path, "R1>R3>192.1.5.0/24");
    CHECK_EQ(rows[2].cost, 4);
    CHECK_EQ(rows[2].next, topo_find(&t, "R2"));
    CHECK_STR(rows[2].path, "R1>R2>192.1.6.0/24");
    CHECK_EQ(rows[3].cost, 8);
    CHECK_EQ(rows[3].next, topo_find(&t, "R2"));
    CHECK_STR(rows[3].path, "R1>R2>R4>192.1.7.0/24");
}

static int rtr(Rip *r, const char *name) {
    return rip_add_router(r, name, err, (int)sizeof err);
}

static int net_of(Rip *r, const char *name) {
    int id = rip_find_net(r, name);
    if (id >= 0) {
        return id;
    }
    return rip_add_net(r, name, err, (int)sizeof err);
}

static void check_rules(void) {
    Rip r;
    int u;
    int n1, n2, n3, n4, n5;
    int r2, r3;

    rip_init(&r);
    CHECK(rtr(&r, "R1") == 0);
    CHECK(rtr(&r, "R2") == 1);
    CHECK(rtr(&r, "R3") == 2);
    CHECK(rip_rlink(&r, 0, 1, err, (int)sizeof err) == 0);
    CHECK(rip_rlink(&r, 0, 2, err, (int)sizeof err) == 0);
    n1 = net_of(&r, "Net1");
    n2 = net_of(&r, "Net2");
    n3 = net_of(&r, "Net3");
    n4 = net_of(&r, "Net4");
    n5 = net_of(&r, "Net5");
    r2 = 1;
    r3 = 2;
    u = 0;
    CHECK(rip_set(&r, u, n1, 1, RIP_DIRECT, err, (int)sizeof err) == 0);
    CHECK(rip_set(&r, u, n2, 3, r2, err, (int)sizeof err) == 0);
    CHECK(rip_set(&r, u, n3, 4, r3, err, (int)sizeof err) == 0);
    CHECK(rip_set(&r, u, n4, 5, r2, err, (int)sizeof err) == 0);
    CHECK(rip_learn(&r, u, r2, n1, 2, err, (int)sizeof err) == 0);
    CHECK(rip_learn(&r, u, r2, n2, 1, err, (int)sizeof err) == 0);
    CHECK(rip_learn(&r, u, r2, n3, 2, err, (int)sizeof err) == 0);
    CHECK(rip_learn(&r, u, r2, n4, 6, err, (int)sizeof err) == 0);
    CHECK(rip_learn(&r, u, r2, n5, 3, err, (int)sizeof err) == 0);
    CHECK_EQ(rip_metric(&r, u, n1), 1);
    CHECK_EQ(rip_via(&r, u, n1), RIP_DIRECT);
    CHECK_EQ(rip_metric(&r, u, n2), 2);
    CHECK_EQ(rip_via(&r, u, n2), r2);
    CHECK_EQ(rip_metric(&r, u, n3), 3);
    CHECK_EQ(rip_via(&r, u, n3), r2);
    CHECK_EQ(rip_metric(&r, u, n4), 7);
    CHECK_EQ(rip_via(&r, u, n4), r2);
    CHECK_EQ(rip_metric(&r, u, n5), 4);
    CHECK_EQ(rip_via(&r, u, n5), r2);
}

static void check_2016(void) {
    Rip r;
    int net;

    rip_init(&r);
    CHECK(rtr(&r, "R1") == 0);
    CHECK(rtr(&r, "R2") == 1);
    CHECK(rtr(&r, "R3") == 2);
    net = net_of(&r, "201.1.2.0/25");
    CHECK(rip_rlink(&r, 0, 1, err, (int)sizeof err) == 0);
    CHECK(rip_rlink(&r, 0, 2, err, (int)sizeof err) == 0);
    CHECK(rip_rlink(&r, 1, 2, err, (int)sizeof err) == 0);
    CHECK(rip_nlink(&r, 2, net, err, (int)sizeof err) == 0);
    rip_relax(&r);
    CHECK_EQ(rip_metric(&r, 0, net), 2);
    CHECK_EQ(rip_via(&r, 0, net), 2);
    CHECK_EQ(rip_metric(&r, 1, net), 2);
    CHECK_EQ(rip_via(&r, 1, net), 2);
    CHECK_EQ(rip_metric(&r, 2, net), 1);
    CHECK(rip_down(&r, 2, net, err, (int)sizeof err) == 0);
    CHECK(rip_hear(&r, 1, 2, err, (int)sizeof err) == 0);
    CHECK_EQ(rip_metric(&r, 1, net), 3);
    CHECK_EQ(rip_via(&r, 1, net), 0);
}

static void link_routers(Rip *r, const char *a, const char *b) {
    CHECK(rip_rlink(r, rip_find_router(r, a), rip_find_router(r, b), err, (int)sizeof err) == 0);
}

static void build_2024(Rip *r) {
    const char *names[] = {"R11", "R12", "R13", "R14", "R15", "R16"};

    rip_init(r);
    for (int i = 0; i < 6; i++) {
        CHECK(rtr(r, names[i]) == i);
    }
    link_routers(r, "R11", "R12");
    link_routers(r, "R12", "R13");
    link_routers(r, "R11", "R14");
    link_routers(r, "R14", "R15");
    link_routers(r, "R15", "R16");
    link_routers(r, "R13", "R15");
    link_routers(r, "R13", "R16");
    CHECK(net_of(r, "210.2.4.0/24") == 0);
    CHECK(net_of(r, "210.2.3.0/24") == 1);
    CHECK(rip_nlink(r, rip_find_router(r, "R16"), 0, err, (int)sizeof err) == 0);
    CHECK(rip_nlink(r, rip_find_router(r, "R14"), 1, err, (int)sizeof err) == 0);
}

static int hit_of(const RipHit *hit, int n, int router, int *metric, int *via) {
    for (int i = 0; i < n; i++) {
        if (hit[i].router == router) {
            *metric = hit[i].metric;
            *via = hit[i].via;
            return hit[i].t;
        }
    }
    return -2;
}

static void check_2024(void) {
    Rip r;
    RipHit hit[8];
    int n = 0;
    int sec = 0;
    int metric = 0;
    int via = 0;

    build_2024(&r);
    CHECK(rip_spread(&r, 0, 30, hit, 8, &n, &sec) == 0);
    CHECK_EQ(sec, 60);
    CHECK_EQ(hit_of(hit, n, 5, &metric, &via), -1);
    CHECK_EQ(metric, 1);
    CHECK_EQ(hit_of(hit, n, 0, &metric, &via), 60);
    CHECK_EQ(metric, 4);
    CHECK_EQ(via, 1);
    CHECK(rip_spread(&r, 1, 30, hit, 8, &n, &sec) == 0);
    CHECK_EQ(sec, 30);
    CHECK_STR(rip_pick_proto(20), "OSPF");
    CHECK_STR(rip_pick_proto(15), "RIP");
    CHECK_EQ(rip_min_ttl(15), 16);
}

static void two_routers(Rip *r, int split) {
    rip_init(r);
    r->split = split;
    CHECK(rtr(r, "R1") == 0);
    CHECK(rtr(r, "R2") == 1);
    CHECK(net_of(r, "N") == 0);
    CHECK(rip_rlink(r, 0, 1, err, (int)sizeof err) == 0);
    CHECK(rip_nlink(r, 0, 0, err, (int)sizeof err) == 0);
    rip_relax(r);
}

static void check_slow(void) {
    Rip plain;
    Rip split;
    SlowStep step[24];
    int n = 0;

    two_routers(&plain, 0);
    CHECK(rip_slow(&plain, 0, 0, step, 24, &n, err, (int)sizeof err) == 0);
    CHECK_EQ(n, 16);
    CHECK_EQ(step[1].metric[0], 3);
    CHECK_EQ(step[1].metric[1], 2);
    CHECK_EQ(step[1].via[0], 1);
    CHECK_EQ(step[2].metric[1], 4);
    CHECK_EQ(step[15].metric[0], 16);
    CHECK_EQ(step[15].metric[1], 16);

    two_routers(&split, 1);
    CHECK(rip_slow(&split, 0, 0, step, 24, &n, err, (int)sizeof err) == 0);
    CHECK_EQ(n, 3);
    CHECK(step[1].suppressed);
    CHECK_EQ(step[1].metric[0], 16);
    CHECK_EQ(step[1].metric[1], 2);
    CHECK_EQ(step[2].metric[0], 16);
    CHECK_EQ(step[2].metric[1], 16);
}

static void check_sdn(void) {
    Sdn s;

    sdn_init(&s);
    CHECK(sdn_add_switch(&s, "S0", err, (int)sizeof err) == 0);
    CHECK(sdn_add_switch(&s, "S1", err, (int)sizeof err) == 1);
    CHECK(sdn_add_switch(&s, "S2", err, (int)sizeof err) == 2);
    CHECK(sdn_add_host(&s, "H0", "S0", err, (int)sizeof err) == 0);
    CHECK(sdn_add_host(&s, "H2", "S2", err, (int)sizeof err) == 1);
    CHECK(sdn_link(&s, "S0", "S1", 1, err, (int)sizeof err) == 0);
    CHECK(sdn_link(&s, "S1", "S2", 1, err, (int)sizeof err) == 0);
    CHECK(sdn_link(&s, "S0", "S2", 1, err, (int)sizeof err) == 0);
    CHECK(sdn_install(&s, err, (int)sizeof err) == 0);
    CHECK_EQ(sdn_next(&s, 0, 1), 2);
    CHECK(sdn_fail(&s, "S0", "S2", err, (int)sizeof err) == 0);
    CHECK_EQ(sdn_next(&s, 0, 1), 1);
    CHECK_EQ(sdn_next(&s, 2, 0), 1);
    CHECK_EQ(sdn_next(&s, 1, 1), 2);
}

int main(void) {
    check_tie();
    check_2014();
    check_rules();
    check_2016();
    check_2024();
    check_slow();
    check_sdn();
    return TEST_SUMMARY();
}
