#include "hash.h"

#include "test.h"

static HashTab *build(int kind, int m, int p, int a, int q, const int *keys, int n) {
    HashTab *t = hash_create(kind, m, p, a, q);
    int i;
    CHECK(t != NULL);
    if (t == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        CHECK_EQ(hash_insert(t, keys[i]), HASH_OK);
    }
    return t;
}

static void expect_asl(const HashTab *t, long long sn, long long sd, long long fn, long long fd) {
    HashAsl asl;
    hash_asl(t, 0, &asl);
    CHECK(asl.has_succ);
    CHECK_EQ(asl.succ_num, sn);
    CHECK_EQ(asl.succ_den, sd);
    CHECK_EQ(asl.depend, 0);
    CHECK_EQ(asl.fail_num, fn);
    CHECK_EQ(asl.fail_den, fd);
}

static void expect_full(const HashTab *t, int index, int key) {
    CHECK_EQ(t->state[index], CELL_FULL);
    CHECK_EQ(t->key[index], key);
}

static void test_2010(void) {
    int keys[] = {7, 8, 30, 11, 18, 9, 14};
    int slot[] = {7, 14, -1, 8, -1, 11, 30, 18, 9, -1};
    HashTab *t = build(KIND_LINEAR, 10, 7, 3, 0, keys, 7);
    int i;
    if (t == NULL) {
        return;
    }
    for (i = 0; i < 10; i++) {
        if (slot[i] < 0) {
            CHECK_EQ(t->state[i], CELL_EMPTY);
        } else {
            expect_full(t, i, slot[i]);
        }
    }
    /* 18 落在 7 而不是绕回 0，说明探测按表长 10 取模，失败分母仍是模 7。 */
    expect_asl(t, 12, 7, 18, 7);
    hash_free(t);
}

static void test_2018_2019(void) {
    int a[] = {22, 43, 15};
    int b[] = {87, 40, 30, 6, 11, 22, 98, 20};
    int slot[] = {98, 22, 30, 87, 11, 40, 6, 20, -1, -1, -1};
    HashTab *t = build(KIND_LINEAR, 7, 7, 1, 0, a, 3);
    HashTab *u;
    int i;
    if (t == NULL) {
        return;
    }
    expect_asl(t, 6, 3, 13, 7);
    hash_free(t);
    u = build(KIND_LINEAR, 11, 7, 1, 0, b, 8);
    if (u == NULL) {
        return;
    }
    for (i = 0; i < 11; i++) {
        if (slot[i] < 0) {
            CHECK_EQ(u->state[i], CELL_EMPTY);
        } else {
            expect_full(u, i, slot[i]);
        }
    }
    expect_asl(u, 9, 8, 42, 7);
    CHECK_EQ(hash_run(u), 8);
    hash_free(u);
}

static void test_2024(void) {
    int keys[] = {20, 3, 11, 18, 9, 14, 7};
    HashTab *pos = build(KIND_QPOS, 11, 11, 3, 0, keys, 7);
    HashTab *both = build(KIND_QUAD, 11, 11, 3, 0, keys, 7);
    OpenWalk hit;
    OpenWalk miss;
    if (pos == NULL || both == NULL) {
        hash_free(pos);
        hash_free(both);
        return;
    }
    CHECK_EQ(open_find(pos, 14, &hit), HASH_OK);
    CHECK_EQ(hit.put, 2);
    CHECK_EQ(hit.cmp, 3);
    CHECK_EQ(hit.cell_key[0], 3);
    CHECK_EQ(hit.cell_key[1], 18);
    CHECK_EQ(hit.cell_key[2], 14);
    CHECK_EQ(open_find(pos, 8, &miss), HASH_MISS);
    CHECK_EQ(miss.put, 7);
    CHECK_EQ(miss.cmp, 5);
    CHECK_EQ(pos->state[8], CELL_EMPTY);
    expect_asl(pos, 12, 7, 29, 11);
    /* 双向平方会在 9 的 -1 处看到空位 8，14 不会再到 2。 */
    expect_full(both, 8, 14);
    hash_free(pos);
    hash_free(both);
}

static void test_chain(void) {
    int keys[] = {19, 14, 23, 1, 68, 20, 84, 27, 55, 11, 10, 79};
    HashTab *t = build(KIND_CHAIN, 13, 13, 1, 0, keys, 12);
    HashAsl extra;
    if (t == NULL) {
        return;
    }
    expect_asl(t, 21, 12, 12, 13);
    CHECK_EQ(t->head[1]->key, 79);
    CHECK_EQ(t->head[1]->next->key, 27);
    hash_asl(t, 1, &extra);
    CHECK_EQ(extra.fail_num, 25);
    CHECK_EQ(extra.fail_den, 13);
    hash_free(t);
}

static void test_double(void) {
    int keys[] = {1, 14};
    int pos[HASH_M_MAX];
    HashTab *t = build(KIND_DOUBLE, 13, 13, 1, 5, keys, 2);
    HashTab *cycle = hash_create(KIND_DOUBLE, 8, 8, 1, 6);
    OpenWalk a;
    OpenWalk b;
    HashAsl asl;
    if (t == NULL || cycle == NULL) {
        hash_free(t);
        hash_free(cycle);
        return;
    }
    CHECK_EQ(open_find(t, 27, &a), HASH_MISS);
    CHECK_EQ(a.put, 4);
    CHECK_EQ(open_find(t, 40, &b), HASH_MISS);
    CHECK_EQ(b.put, 6);
    hash_asl(t, 0, &asl);
    CHECK_EQ(asl.depend, 1);
    CHECK_EQ(asl.succ_num, 3);
    CHECK_EQ(asl.succ_den, 2);
    CHECK_EQ(hash_probe(cycle, 6, 6, pos), 4);
    hash_free(t);
    hash_free(cycle);
}

static void test_delete(void) {
    int keys[] = {22, 43, 15};
    HashTab *kept = build(KIND_LINEAR, 7, 7, 1, 0, keys, 3);
    HashTab *wiped = build(KIND_LINEAR, 7, 7, 1, 0, keys, 3);
    OpenWalk w;
    if (kept == NULL || wiped == NULL) {
        hash_free(kept);
        hash_free(wiped);
        return;
    }
    CHECK_EQ(open_delete(kept, 43, 0, &w), HASH_OK);
    CHECK_EQ(kept->state[2], CELL_TOMB);
    CHECK_EQ(open_find(kept, 15, &w), HASH_OK);
    CHECK_EQ(w.cmp, 3);
    CHECK_EQ(open_delete(wiped, 43, 1, &w), HASH_OK);
    CHECK_EQ(open_find(wiped, 15, &w), HASH_MISS);
    CHECK_EQ(w.put, 2);
    hash_free(kept);
    hash_free(wiped);
}

static void test_quad_pair(void) {
    int keys[] = {12, 23, 34, 45};
    HashTab *both = build(KIND_QUAD, 11, 11, 1, 0, keys, 4);
    HashTab *pos = build(KIND_QPOS, 11, 11, 1, 0, keys, 4);
    HashTab *gap = hash_create(KIND_QUAD, 5, 5, 1, 0);
    int seed[] = {0, 5, 10};
    int i;
    if (both == NULL || pos == NULL || gap == NULL) {
        hash_free(both);
        hash_free(pos);
        hash_free(gap);
        return;
    }
    expect_full(both, 0, 34);
    expect_full(both, 5, 45);
    expect_full(pos, 5, 34);
    expect_full(pos, 10, 45);
    expect_asl(both, 10, 4, 19, 11);
    for (i = 0; i < 3; i++) {
        CHECK_EQ(hash_insert(gap, seed[i]), HASH_OK);
    }
    CHECK_EQ(hash_insert(gap, 15), HASH_FULL);
    CHECK_EQ(gap->n, 3);
    CHECK_EQ(gap->state[2], CELL_EMPTY);
    hash_free(both);
    hash_free(pos);
    hash_free(gap);
}

static void test_formula_and_spread(void) {
    HashFormula a;
    HashFormula b;
    HashFormula chain;
    int keys[] = {1, 12, 23, 8, 19};
    int grow_keys[] = {1, 2, 3, 4, 5, 6};
    HashTab *small;
    HashTab *big;
    HashTab *grown;
    CHECK(hash_formula(KIND_LINEAR, 7, 10, &a));
    CHECK(hash_formula(KIND_LINEAR, 14, 20, &b));
    CHECK_EQ(a.succ_num, 13);
    CHECK_EQ(a.succ_den, 6);
    CHECK_EQ(a.fail_num, 109);
    CHECK_EQ(a.fail_den, 18);
    CHECK(a.succ_num * b.succ_den == b.succ_num * a.succ_den);
    CHECK(a.fail_num * b.fail_den == b.fail_num * a.fail_den);
    CHECK(hash_formula(KIND_CHAIN, 7, 10, &chain));
    CHECK_EQ(chain.succ_num, 27);
    CHECK_EQ(chain.succ_den, 20);
    small = build(KIND_LINEAR, 11, 11, 1, 0, keys, 5);
    big = build(KIND_LINEAR, 23, 23, 1, 0, keys, 5);
    grown = build(KIND_LINEAR, 8, 5, 1, 0, grow_keys, 6);
    if (small == NULL || big == NULL || grown == NULL) {
        hash_free(small);
        hash_free(big);
        hash_free(grown);
        return;
    }
    expect_asl(small, 9, 5, 20, 11);
    expect_asl(big, 5, 5, 29, 23);
    expect_full(big, 0, 23);
    expect_asl(grown, 10, 6, 25, 5);
    hash_free(small);
    hash_free(big);
    hash_free(grown);
}

static void test_2023(void) {
    int keys[] = {2022, 12, 25};
    HashTab *t = hash_create(KIND_LINEAR, 5, 5, 1, 0);
    OpenWalk w;
    int i;
    CHECK(t != NULL);
    if (t == NULL) {
        return;
    }
    t->b = 4;
    for (i = 0; i < 3; i++) {
        CHECK_EQ(hash_insert(t, keys[i]), HASH_OK);
    }
    expect_full(t, 1, 2022);
    expect_full(t, 2, 12);
    expect_full(t, 4, 25);
    CHECK_EQ(open_delete(t, 25, 0, &w), HASH_OK);
    CHECK_EQ(t->state[4], CELL_TOMB);
    expect_asl(t, 3, 2, 9, 5);
    hash_free(t);
}

int main(void) {
    test_2010();
    test_2018_2019();
    test_2024();
    test_chain();
    test_double();
    test_delete();
    test_quad_pair();
    test_formula_and_spread();
    test_2023();
    return TEST_SUMMARY();
}
