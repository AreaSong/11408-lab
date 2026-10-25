#include "bank.h"
#include "bound.h"
#include "exam.h"
#include "graph.h"
#include "phi.h"
#include "recover.h"
#include "test.h"

#include <string.h>

static void set_bank(Bank *b, int n, int m, int base, const int *avail, int kind_need,
                     const int rows[][BANK_M], const int alloc[][BANK_M]) {
    int i;
    memset(b, 0, sizeof *b);
    b->n = n;
    b->m = m;
    b->base = base;
    num_copy(b->avail, avail, m);
    for (i = 0; i < n; i++) {
        num_copy(b->alloc[i], alloc[i], m);
    }
    CHECK(bank_bind(b, kind_need, rows) == 0);
}

static void test_2011(void) {
    Bank b;
    SafeTrace t;
    const int avail[] = {0, 2, 1};
    const int need[][BANK_M] = {{0, 0, 1}, {1, 3, 2}, {1, 3, 1}, {2, 0, 0}};
    const int alloc[][BANK_M] = {{2, 0, 0}, {1, 2, 0}, {0, 1, 1}, {0, 0, 1}};
    int total = 0;
    set_bank(&b, 4, 3, 1, avail, 1, need, alloc);
    bank_safe(&b, &t);
    CHECK_EQ(t.safe, 0);
    CHECK_EQ(t.nstep, 2);
    CHECK_EQ(t.order[0], 0);
    CHECK_EQ(t.order[1], 3);
    CHECK_EQ(t.steps[0].next[0], 2);
    CHECK_EQ(t.steps[0].next[1], 2);
    CHECK_EQ(t.steps[0].next[2], 1);
    CHECK_EQ(t.stuck[0], 2);
    CHECK_EQ(t.stuck[1], 2);
    CHECK_EQ(t.stuck[2], 2);
    CHECK_EQ(t.nopen, 2);
    bank_collect(&b, &total, NULL, 0);
    CHECK_EQ(total, 0);
}

static void test_2012(void) {
    Bank b;
    SafeTrace t;
    const int total_res[] = {18, 6, 22};
    const int maxv[][BANK_M] = {{5, 5, 10}, {5, 3, 6}, {4, 0, 11}, {4, 2, 5}, {4, 2, 4}};
    const int alloc[][BANK_M] = {{3, 2, 3}, {4, 0, 3}, {4, 0, 5}, {2, 0, 4}, {3, 1, 4}};
    const int opt_a[] = {0, 2, 4, 1, 3};
    const int opt_b[] = {1, 0, 3, 4, 2};
    const int opt_c[] = {2, 1, 0, 3, 4};
    const int opt_d[] = {3, 4, 2, 1, 0};
    int fail = -1;
    int total = 0;
    int avail_zero[] = {0, 0, 0};
    set_bank(&b, 5, 3, 0, avail_zero, 0, maxv, alloc);
    CHECK(bank_from_total(&b, total_res) == 0);
    CHECK_EQ(b.avail[0], 2);
    CHECK_EQ(b.avail[1], 3);
    CHECK_EQ(b.avail[2], 3);
    CHECK_EQ(b.need[0][0], 2);
    CHECK_EQ(b.need[0][1], 3);
    CHECK_EQ(b.need[0][2], 7);
    CHECK_EQ(b.need[4][0], 1);
    CHECK_EQ(b.need[4][1], 1);
    CHECK_EQ(b.need[4][2], 0);
    bank_safe(&b, &t);
    CHECK_EQ(t.safe, 1);
    CHECK_EQ(t.order[0], 1);
    CHECK_EQ(t.order[1], 2);
    CHECK_EQ(t.order[2], 0);
    CHECK_EQ(t.order[3], 3);
    CHECK_EQ(t.order[4], 4);
    CHECK_EQ(t.steps[0].next[0], 6);
    CHECK_EQ(t.steps[0].next[1], 3);
    CHECK_EQ(t.steps[0].next[2], 6);
    CHECK_EQ(bank_check(&b, opt_a, &fail), 0);
    CHECK_EQ(fail, 0);
    CHECK_EQ(bank_check(&b, opt_b, &fail), 0);
    CHECK_EQ(fail, 0);
    CHECK_EQ(bank_check(&b, opt_c, &fail), 0);
    CHECK_EQ(fail, 2);
    CHECK_EQ(bank_check(&b, opt_d, &fail), 1);
    bank_collect(&b, &total, NULL, 0);
    CHECK_EQ(total, 66);
}

static void test_2018(void) {
    Bank b;
    SafeTrace t;
    const int avail[] = {1};
    const int maxv[][BANK_M] = {{4}, {3}, {1}};
    const int alloc[][BANK_M] = {{2}, {1}, {0}};
    set_bank(&b, 3, 1, 1, avail, 0, maxv, alloc);
    bank_safe(&b, &t);
    CHECK_EQ(b.need[0][0], 2);
    CHECK_EQ(b.need[1][0], 2);
    CHECK_EQ(b.need[2][0], 1);
    CHECK_EQ(t.safe, 0);
    CHECK_EQ(t.nstep, 1);
    CHECK_EQ(t.order[0], 2);
    CHECK_EQ(t.stuck[0], 1);
}

static void test_2022(void) {
    Bank b;
    const int avail[] = {1, 3, 2};
    const int need[][BANK_M] = {{0, 2, 1}, {1, 2, 3}, {0, 1, 3}};
    const int alloc[][BANK_M] = {{2, 0, 1}, {0, 2, 0}, {1, 0, 1}};
    int seqs[4][BANK_N];
    int total = 0;
    SafeTrace t;
    set_bank(&b, 3, 3, 0, avail, 1, need, alloc);
    bank_safe(&b, &t);
    CHECK_EQ(t.safe, 1);
    CHECK_EQ(t.order[0], 0);
    CHECK_EQ(t.order[1], 1);
    CHECK_EQ(t.order[2], 2);
    CHECK_EQ(t.steps[0].next[0], 3);
    CHECK_EQ(t.steps[0].next[1], 3);
    CHECK_EQ(t.steps[0].next[2], 3);
    bank_collect(&b, &total, seqs, 4);
    CHECK_EQ(total, 2);
    CHECK_EQ(seqs[0][0], 0);
    CHECK_EQ(seqs[0][1], 1);
    CHECK_EQ(seqs[0][2], 2);
    CHECK_EQ(seqs[1][0], 0);
    CHECK_EQ(seqs[1][1], 2);
    CHECK_EQ(seqs[1][2], 1);
}

static void textbook(Bank *b) {
    const int avail[] = {3, 3, 2};
    const int maxv[][BANK_M] = {{7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}};
    const int alloc[][BANK_M] = {{0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}};
    set_bank(b, 5, 3, 0, avail, 0, maxv, alloc);
}

static void test_request(void) {
    Bank b;
    ReqResult r;
    const int grant[] = {1, 0, 2};
    const int unsafe[] = {3, 3, 0};
    const int wait[] = {0, 4, 0};
    const int exceed[] = {2, 0, 0};
    SafeTrace t;
    textbook(&b);
    bank_safe(&b, &t);
    CHECK_EQ(t.safe, 1);
    CHECK_EQ(t.order[0], 1);
    bank_request(&b, 1, grant, &r);
    CHECK_EQ(r.kind, REQ_GRANT);
    CHECK_EQ(r.trace.order[0], 1);
    CHECK_EQ(r.trace.order[1], 3);
    CHECK_EQ(r.trace.order[2], 0);
    CHECK_EQ(r.trace.order[3], 2);
    CHECK_EQ(r.trace.order[4], 4);
    CHECK_EQ(r.trial_avail[0], 2);
    CHECK_EQ(r.trial_avail[1], 3);
    CHECK_EQ(r.trial_avail[2], 0);
    CHECK_EQ(b.avail[0], 3);
    bank_request(&b, 4, unsafe, &r);
    CHECK_EQ(r.kind, REQ_UNSAFE);
    CHECK_EQ(r.trace.nstep, 0);
    bank_request(&b, 0, wait, &r);
    CHECK_EQ(r.kind, REQ_WAIT);
    bank_request(&b, 1, exceed, &r);
    CHECK_EQ(r.kind, REQ_EXCEED);
}

static void test_after_grant_unsafe(void) {
    Bank b;
    ReqResult r;
    const int avail[] = {2, 3, 0};
    const int maxv[][BANK_M] = {{7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}};
    const int alloc[][BANK_M] = {{0, 1, 0}, {3, 0, 2}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}};
    const int ask[] = {0, 2, 0};
    set_bank(&b, 5, 3, 0, avail, 0, maxv, alloc);
    bank_request(&b, 0, ask, &r);
    CHECK_EQ(r.kind, REQ_UNSAFE);
    CHECK_EQ(r.trace.nstep, 0);
}

static void test_graph(void) {
    Graph g;
    RedTrace t;
    memset(&g, 0, sizeof g);
    g.n = 3;
    g.m = 3;
    g.base = 0;
    g.alloc[0][0] = 1;
    g.alloc[1][1] = 1;
    g.alloc[2][2] = 1;
    g.req[0][1] = 1;
    g.req[1][2] = 1;
    g.req[2][0] = 1;
    graph_reduce(&g, &t);
    CHECK_EQ(t.deadlock, 1);
    CHECK_EQ(t.nleft, 3);
    CHECK_EQ(t.nstep, 0);

    memset(&g, 0, sizeof g);
    g.n = 3;
    g.m = 2;
    g.alloc[0][0] = 1;
    g.alloc[1][1] = 1;
    g.alloc[2][0] = 1;
    g.req[0][1] = 1;
    g.req[1][0] = 1;
    graph_reduce(&g, &t);
    CHECK_EQ(t.deadlock, 0);
    CHECK_EQ(t.nremoved, 3);
    CHECK_EQ(t.removed[0], 2);
    CHECK_EQ(t.removed[1], 1);
    CHECK_EQ(t.removed[2], 0);
}

static void test_2016(void) {
    Graph g;
    RedTrace t;
    memset(&g, 0, sizeof g);
    g.n = 4;
    g.m = 3;
    g.base = 1;
    g.alloc[0][0] = 1;
    g.alloc[1][1] = 1;
    g.alloc[2][2] = 1;
    g.req[0][1] = 1;
    g.req[1][2] = 1;
    g.req[2][0] = 1;
    graph_reduce(&g, &t);
    CHECK_EQ(t.deadlock, 1);
    CHECK_EQ(t.niso, 1);
    CHECK_EQ(t.isolated[0], 3);
    CHECK_EQ(t.nleft, 3);
    CHECK_EQ(t.left[0], 0);
    CHECK_EQ(t.left[1], 1);
    CHECK_EQ(t.left[2], 2);
}

static void test_recover(void) {
    Graph g;
    RecResult r;
    int cost[] = {4, 1, 9};
    int tie[] = {2, 2, 7};
    memset(&g, 0, sizeof g);
    g.n = 3;
    g.m = 3;
    g.alloc[0][0] = 1;
    g.alloc[1][1] = 1;
    g.alloc[2][2] = 1;
    g.req[0][1] = 1;
    g.req[1][2] = 1;
    g.req[2][0] = 1;
    recover_run(&g, cost, &r);
    CHECK_EQ(r.recovered, 1);
    CHECK_EQ(r.naborts, 1);
    CHECK_EQ(r.abort_who[0], 1);
    CHECK_EQ(r.abort_cost[0], 1);
    recover_run(&g, tie, &r);
    CHECK_EQ(r.recovered, 1);
    CHECK_EQ(r.abort_who[0], 0);

    memset(&g, 0, sizeof g);
    g.n = 2;
    g.m = 1;
    g.alloc[1][0] = 1;
    g.req[0][0] = 1;
    g.req[1][0] = 1;
    cost[0] = 1;
    cost[1] = 100;
    recover_run(&g, cost, &r);
    CHECK_EQ(r.recovered, 1);
    CHECK_EQ(r.naborts, 1);
    CHECK_EQ(r.abort_who[0], 1);
}

static void test_unsafe_not_dead(void) {
    Bank b;
    Graph g;
    SafeTrace s;
    RedTrace d;
    const int avail[] = {0};
    const int maxv[][BANK_M] = {{3}, {3}, {3}};
    const int alloc[][BANK_M] = {{1}, {1}, {1}};
    set_bank(&b, 3, 1, 0, avail, 0, maxv, alloc);
    bank_safe(&b, &s);
    CHECK_EQ(s.safe, 0);
    memset(&g, 0, sizeof g);
    g.n = 3;
    g.m = 1;
    g.alloc[0][0] = 1;
    g.alloc[1][0] = 1;
    g.alloc[2][0] = 1;
    graph_reduce(&g, &d);
    CHECK_EQ(d.deadlock, 0);
    g.req[0][0] = 1;
    g.req[1][0] = 1;
    g.req[2][0] = 1;
    graph_reduce(&g, &d);
    CHECK_EQ(d.deadlock, 1);
    CHECK_EQ(d.nleft, 3);
}

static void test_bound(void) {
    long long least = 0;
    long long worst = 0;
    int out = 0;
    const int ks[] = {3, 4, 5};
    CHECK(bound_mink(8, 3, &out) == 0);
    CHECK_EQ(out, 4);
    CHECK(bound_maxn(8, 3, &out) == 0);
    CHECK_EQ(out, 3);
    CHECK(bound_list(ks, 3, &least, &worst) == 0);
    CHECK_EQ(worst, 9);
    CHECK_EQ(least, 10);
    CHECK(bound_equal(5, 2, &least) == 0);
    CHECK_EQ(least, 6);
    CHECK(bound_mink(0, 3, &out) != 0);
    CHECK(bound_equal(0, 2, &least) != 0);
}

static void test_phi(void) {
    PhiResult r;
    CHECK(phi_run(PHI_CLASSIC, 5, &r) == 0);
    CHECK_EQ(r.deadlock, 1);
    CHECK_EQ(r.seen_hold, 1);
    CHECK_EQ(r.seen_cycle, 1);
    CHECK_STR(r.breaks, "none");
    CHECK(phi_run(PHI_ORDER, 5, &r) == 0);
    CHECK_EQ(r.deadlock, 0);
    CHECK_EQ(r.seen_hold, 1);
    CHECK_EQ(r.seen_cycle, 0);
    CHECK_STR(r.breaks, "cycle");
    CHECK(phi_run(PHI_ALL, 5, &r) == 0);
    CHECK_EQ(r.deadlock, 0);
    CHECK_EQ(r.seen_hold, 0);
    CHECK_EQ(r.seen_cycle, 0);
    CHECK_STR(r.breaks, "hold");
    CHECK(phi_run(PHI_CLASSIC, 1, &r) != 0);
}

static void test_claims(void) {
    CHECK_STR(exam_claim("s1-limit-order"), "false");
    CHECK_STR(exam_claim("s1-need-max"), "true");
    CHECK_STR(exam_claim("s1-refuse-unsafe"), "true");
    CHECK_STR(exam_claim("banker-detects"), "false");
    CHECK_STR(exam_claim("deprive-recover"), "true");
    CHECK_STR(exam_claim("prevent-ensures"), "true");
    CHECK_STR(exam_claim("deadlock-two-block"), "true");
    CHECK_STR(exam_claim("unsafe-is-dead"), "false");
    CHECK_STR(exam_claim("cycle-many-enough"), "false");
    CHECK(exam_claim("no-such") == NULL);
    CHECK(exam_card_count() >= 8);
}

int main(void) {
    test_2011();
    test_2012();
    test_2018();
    test_2022();
    test_request();
    test_after_grant_unsafe();
    test_graph();
    test_2016();
    test_recover();
    test_unsafe_not_dead();
    test_bound();
    test_phi();
    test_claims();
    return TEST_SUMMARY();
}
