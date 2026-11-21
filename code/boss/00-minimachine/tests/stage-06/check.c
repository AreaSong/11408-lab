#include "kernel/k_bank.h"
#include "kernel/k_dead.h"
#include "kernel/k_pv.h"
#include "kernel/k_sync.h"
#include "test.h"

#include <string.h>

void test_live(void);

static void set_bank(KBank *b, int n, int m, int base, const int *avail, int kind_need, const int rows[][K_BANK_M],
                     const int alloc[][K_BANK_M]) {
    int i;
    memset(b, 0, sizeof *b);
    b->n = n;
    b->m = m;
    b->base = base;
    memcpy(b->avail, avail, (size_t)m * sizeof avail[0]);
    for (i = 0; i < n; i++) {
        memcpy(b->alloc[i], alloc[i], (size_t)m * sizeof(int));
        if (kind_need) {
            memcpy(b->need[i], rows[i], (size_t)m * sizeof(int));
        } else {
            memcpy(b->maxv[i], rows[i], (size_t)m * sizeof(int));
        }
    }
    CHECK(k_bank_finish(b, kind_need) == 0);
}

static void test_sem_and_race(void) {
    KSemLog s;
    memset(&s, 0, sizeof s);
    s.value = 1;
    CHECK(k_sem_ok(1, 0));
    CHECK(k_sem_pv(&s, 0, 1) == 0);
    CHECK_EQ(s.value, 0);
    CHECK(k_sem_pv(&s, 0, 2) == 1);
    CHECK_EQ(s.value, -1);
    CHECK_EQ(s.nwait, 1);
    CHECK(k_sem_ok(s.value, s.nwait));
    CHECK_EQ(k_sem_pv(&s, 1, 3), 2);
    CHECK_EQ(s.value, 0);
    CHECK_EQ(s.nwait, 0);
    CHECK(k_sem_ok(-2, 1) == 0);
    CHECK_EQ(k_race_run(4, 0), 4);
    CHECK_EQ(k_race_run(4, 1), 8);
    CHECK(k_kernel_if(MM_PSW_U | MM_PSW_IF));
}

static void test_2011_2018(void) {
    KBank b;
    KSafe t;
    const int avail11[] = {0, 2, 1};
    const int need11[][K_BANK_M] = {{0, 0, 1}, {1, 3, 2}, {1, 3, 1}, {2, 0, 0}};
    const int alloc11[][K_BANK_M] = {{2, 0, 0}, {1, 2, 0}, {0, 1, 1}, {0, 0, 1}};
    const int avail18[] = {1};
    const int max18[][K_BANK_M] = {{4}, {3}, {1}};
    const int alloc18[][K_BANK_M] = {{2}, {1}, {0}};
    set_bank(&b, 4, 3, 1, avail11, 1, need11, alloc11);
    CHECK(k_bank_safe(&b, &t) == 0);
    CHECK_EQ(t.safe, 0);
    CHECK_EQ(t.nstep, 2);
    CHECK_EQ(t.order[0], 0);
    CHECK_EQ(t.order[1], 3);
    CHECK_EQ(t.stuck[0], 2);
    CHECK_EQ(t.stuck[2], 2);
    CHECK_EQ(k_bank_count(&b), 0);
    set_bank(&b, 3, 1, 1, avail18, 0, max18, alloc18);
    CHECK(k_bank_safe(&b, &t) == 0);
    CHECK_EQ(b.need[2][0], 1);
    CHECK_EQ(t.safe, 0);
    CHECK_EQ(t.nstep, 1);
    CHECK_EQ(t.order[0], 2);
    CHECK_EQ(t.stuck[0], 1);
}

static void test_2012(void) {
    KBank b;
    KSafe t;
    const int total[] = {18, 6, 22};
    const int maxv[][K_BANK_M] = {{5, 5, 10}, {5, 3, 6}, {4, 0, 11}, {4, 2, 5}, {4, 2, 4}};
    const int alloc[][K_BANK_M] = {{3, 2, 3}, {4, 0, 3}, {4, 0, 5}, {2, 0, 4}, {3, 1, 4}};
    const int opt_d[] = {3, 4, 2, 1, 0};
    const int opt_a[] = {0, 2, 4, 1, 3};
    int fail = -1;
    int zero[] = {0, 0, 0};
    set_bank(&b, 5, 3, 0, zero, 0, maxv, alloc);
    CHECK(k_bank_from_total(&b, total) == 0);
    CHECK_EQ(b.avail[0], 2);
    CHECK_EQ(b.avail[2], 3);
    CHECK_EQ(b.need[0][1], 3);
    CHECK_EQ(b.need[0][2], 7);
    CHECK_EQ(b.need[4][2], 0);
    CHECK(k_bank_safe(&b, &t) == 0);
    CHECK_EQ(t.safe, 1);
    CHECK_EQ(t.order[0], 1);
    CHECK_EQ(t.order[1], 2);
    CHECK_EQ(t.order[2], 0);
    CHECK_EQ(t.order[3], 3);
    CHECK_EQ(t.order[4], 4);
    CHECK_EQ(t.next[0][0], 6);
    CHECK_EQ(t.next[0][2], 6);
    CHECK_EQ(k_bank_check(&b, opt_d, 5, &fail), 1);
    CHECK_EQ(k_bank_check(&b, opt_a, 5, &fail), 0);
    CHECK_EQ(fail, 0);
    CHECK_EQ(k_bank_count(&b), 66);
}

static void test_2022_and_request(void) {
    KBank b;
    KSafe t;
    int seqs[4][K_BANK_N];
    int total = 0;
    int kind = -1;
    const int avail[] = {1, 3, 2};
    const int need[][K_BANK_M] = {{0, 2, 1}, {1, 2, 3}, {0, 1, 3}};
    const int alloc[][K_BANK_M] = {{2, 0, 1}, {0, 2, 0}, {1, 0, 1}};
    const int book_av[] = {3, 3, 2};
    const int book_max[][K_BANK_M] = {{7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}};
    const int book_al[][K_BANK_M] = {{0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}};
    const int grant[] = {1, 0, 2};
    const int unsafe[] = {3, 3, 0};
    const int wait[] = {0, 4, 0};
    const int exceed[] = {2, 0, 0};
    set_bank(&b, 3, 3, 0, avail, 1, need, alloc);
    CHECK(k_bank_safe(&b, &t) == 0);
    CHECK_EQ(t.order[0], 0);
    CHECK_EQ(t.order[1], 1);
    CHECK_EQ(t.next[0][0], 3);
    CHECK(k_bank_seqs(&b, seqs, 4, &total) == 0);
    CHECK_EQ(total, 2);
    CHECK_EQ(seqs[1][1], 2);
    set_bank(&b, 5, 3, 0, book_av, 0, book_max, book_al);
    CHECK(k_bank_safe(&b, &t) == 0);
    CHECK_EQ(t.order[0], 1);
    CHECK(k_bank_request(&b, 1, grant, &kind, &t) == 0);
    CHECK_EQ(kind, K_GRANT);
    CHECK_EQ(t.order[1], 3);
    CHECK_EQ(t.order[4], 4);
    CHECK_EQ(b.avail[0], 3);
    CHECK(k_bank_request(&b, 4, unsafe, &kind, &t) == 0);
    CHECK_EQ(kind, K_UNSAFE);
    CHECK(k_bank_request(&b, 0, wait, &kind, &t) == 0);
    CHECK_EQ(kind, K_WAIT);
    CHECK(k_bank_request(&b, 1, exceed, &kind, &t) == 0);
    CHECK_EQ(kind, K_EXCEED);
}

static void test_bound_claim(void) {
    long long least = 0;
    int mink = 0;
    const int ks[] = {3, 4, 5};
    CHECK(k_bound_list(ks, 3, &least) == 0);
    CHECK_EQ(least, 10);
    CHECK(k_bound_mink(8, 3, &mink) == 0);
    CHECK_EQ(mink, 4);
    CHECK(k_bound_equal(5, 2, &least) == 0);
    CHECK_EQ(least, 6);
    CHECK_STR(k_claim("s1-limit-order"), "false");
    CHECK_STR(k_claim("s1-refuse-unsafe"), "true");
    CHECK_STR(k_claim("banker-detects"), "false");
    CHECK_STR(k_claim("deprive-recover"), "true");
    CHECK_STR(k_claim("cycle-many-enough"), "false");
    CHECK(k_claim("no-such") == NULL);
}

static void test_graph_and_bound(void) {
    KRag g;
    KRed red;
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
    CHECK(k_rag_reduce(&g, &red) == 0);
    CHECK_EQ(red.deadlock, 1);
    CHECK_EQ(red.cycle, 1);
    CHECK_EQ(red.nleft, 3);
    CHECK_EQ(red.niso, 1);
    CHECK_EQ(red.isolated[0], 3);
    memset(&g, 0, sizeof g);
    g.n = 3;
    g.m = 2;
    g.alloc[0][0] = 1;
    g.alloc[1][1] = 1;
    g.alloc[2][0] = 1;
    g.req[0][1] = 1;
    g.req[1][0] = 1;
    CHECK(k_rag_reduce(&g, &red) == 0);
    CHECK_EQ(red.deadlock, 0);
    CHECK_EQ(red.cycle, 1);
    CHECK_EQ(red.removed[0], 2);
}

static void test_recover(void) {
    KRag g;
    KRecover rec;
    int cost[] = {4, 1, 9};
    memset(&g, 0, sizeof g);
    g.n = 3;
    g.m = 3;
    g.alloc[0][0] = 1;
    g.alloc[1][1] = 1;
    g.alloc[2][2] = 1;
    g.req[0][1] = 1;
    g.req[1][2] = 1;
    g.req[2][0] = 1;
    CHECK(k_rag_recover(&g, cost, &rec) == 0);
    CHECK_EQ(rec.recovered, 1);
    CHECK_EQ(rec.who[0], 1);
    CHECK_EQ(rec.cost[0], 1);
    cost[0] = 1;
    cost[1] = 100;
    memset(&g, 0, sizeof g);
    g.n = 2;
    g.m = 1;
    g.alloc[1][0] = 1;
    g.req[0][0] = 1;
    g.req[1][0] = 1;
    CHECK(k_rag_recover(&g, cost, &rec) == 0);
    CHECK_EQ(rec.who[0], 1);
}

static void test_pv(void) {
    KPvOut out;
    CHECK(k_pv_run(K_PV_PC, &out) == 0);
    CHECK_EQ(out.deadlock, 0);
    CHECK_EQ(out.produced, 2);
    CHECK_EQ(out.consumed, 2);
    CHECK(k_pv_run(K_PV_WRONG, &out) == 0);
    CHECK_EQ(out.deadlock, 1);
    CHECK_EQ(out.cycle, 1);
    CHECK(k_pv_run(K_PV_PHI, &out) == 0);
    CHECK_EQ(out.deadlock, 1);
    CHECK_EQ(out.cycle, 1);
    CHECK_EQ(out.meals, 0);
    CHECK(k_pv_run(K_PV_ROOM, &out) == 0);
    CHECK_EQ(out.deadlock, 0);
    CHECK_EQ(out.meals, 5);
    CHECK(k_pv_run(K_PV_READ, &out) == 0);
    CHECK_EQ(out.deadlock, 0);
    CHECK(out.max_read >= 2);
}

int main(void) {
    test_sem_and_race();
    test_2011_2018();
    test_2012();
    test_2022_and_request();
    test_graph_and_bound();
    test_recover();
    test_bound_claim();
    test_pv();
    test_live();
    return TEST_SUMMARY();
}
