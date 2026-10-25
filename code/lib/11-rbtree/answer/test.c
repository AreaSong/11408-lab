#include "check.h"
#include "delete.h"
#include "insert.h"
#include "parse.h"

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *shape_of(const RbTree *t) {
    char *buf = NULL;
    size_t n = 0;
    FILE *fp = open_memstream(&buf, &n);
    if (!fp) {
        exit(1);
    }
    rb_fprint(t, t->root, fp);
    fclose(fp);
    return buf;
}

static char *capture_insert(const int *keys, int n, int *last_spins) {
    RbTree *t = rb_create();
    char *buf = NULL;
    size_t size = 0;
    FILE *fp = open_memstream(&buf, &size);
    int spins = 0;
    if (!fp) {
        exit(1);
    }
    for (int i = 0; i < n; i++) {
        spins = 0;
        CHECK(rb_insert(t, keys[i], fp, &spins) == 0);
        CHECK(spins <= 2);
    }
    fclose(fp);
    if (last_spins) {
        *last_spins = spins;
    }
    rb_free(t);
    return buf;
}

static void assert_rb(const RbTree *t) {
    RbStats s;
    rb_stats(t, &s);
    CHECK(s.bst);
    CHECK(s.root_black);
    CHECK(s.nil_black);
    CHECK(s.redred);
    CHECK(s.bh_ok);
    CHECK(s.rb);
    CHECK(s.long_path <= 2 * s.short_path);
    if (s.nodes >= 1) {
        long long two_h = 1LL << s.height;
        long long cap = (long long)(s.nodes + 1) * (s.nodes + 1);
        CHECK(two_h < cap);
    }
}

static RbTree *build(const int *keys, int n) {
    RbTree *t = rb_create();
    for (int i = 0; i < n; i++) {
        int spins = 0;
        CHECK(rb_insert(t, keys[i], NULL, &spins) == 0);
        CHECK(spins <= 2);
        assert_rb(t);
    }
    return t;
}

static void expect_shape(const int *keys, int n, const char *expect) {
    RbTree *t = build(keys, n);
    char *got = shape_of(t);
    CHECK_STR(got, expect);
    free(got);
    rb_free(t);
}

static int has_step(const char *buf, const char *first, const char *second) {
    const char *p = strstr(buf, first);
    return p != NULL && strstr(p, second) != NULL;
}

static void check_kinds(void) {
    static const int ll[] = {30, 20, 10};
    static const int rr[] = {10, 20, 30};
    static const int lr[] = {30, 10, 20};
    static const int rl[] = {10, 30, 20};
    static const char *shape = "20[B](10[R],30[R])";
    int spins = 0;
    char *buf;
    expect_shape(ll, 3, shape);
    expect_shape(rr, 3, shape);
    expect_shape(lr, 3, shape);
    expect_shape(rl, 3, shape);

    buf = capture_insert(ll, 3, &spins);
    CHECK_EQ(spins, 1);
    CHECK(has_step(buf, "uncle # B kind=LL\n", "rotate 30 right\n"));
    free(buf);
    buf = capture_insert(rr, 3, &spins);
    CHECK_EQ(spins, 1);
    CHECK(has_step(buf, "uncle # B kind=RR\n", "rotate 10 left\n"));
    free(buf);
    buf = capture_insert(lr, 3, &spins);
    CHECK_EQ(spins, 2);
    CHECK(has_step(buf, "uncle # B kind=LR\n", "rotate 10 left\n"));
    CHECK(has_step(buf, "rotate 10 left\n", "rotate 30 right\n"));
    free(buf);
    buf = capture_insert(rl, 3, &spins);
    CHECK_EQ(spins, 2);
    CHECK(has_step(buf, "uncle # B kind=RL\n", "rotate 30 right\n"));
    CHECK(has_step(buf, "rotate 30 right\n", "rotate 10 left\n"));
    free(buf);
}

static void check_sequence(void) {
    static const int up[] = {1, 2, 3, 4, 5, 6, 7, 8};
    static const int neg[] = {-2, -1, 0};
    RbTree *t = build(up, 7);
    RbStats s;
    char *got = shape_of(t);
    char *buf;
    const char *p;
    CHECK_STR(got, "2[B](1[B],4[R](3[B],6[B](5[R],7[R])))");
    rb_stats(t, &s);
    CHECK_EQ(s.reds, 3);
    CHECK_EQ(s.bh, 2);
    CHECK_EQ(s.height, 4);
    CHECK_EQ(s.long_path, 4);
    CHECK_EQ(s.short_path, 2);
    free(got);
    rb_free(t);

    t = build(up, 8);
    got = shape_of(t);
    CHECK_STR(got, "4[B](2[R](1[B],3[B]),6[R](5[B],7[B](#,8[R])))");
    free(got);
    rb_free(t);

    buf = capture_insert(up, 4, NULL);
    CHECK(has_step(buf, "uncle 1 R kind=recolor\n", "root 2 B\n"));
    free(buf);
    buf = capture_insert(up, 8, NULL);
    p = strstr(buf, "uncle 5 R kind=recolor\n");
    CHECK(p != NULL);
    CHECK(strstr(p, "uncle 1 B kind=RR\n") != NULL);
    CHECK(strstr(p, "rotate 2 left\n") != NULL);
    free(buf);
    expect_shape(neg, 3, "-1[B](-2[R],0[R])");
}

static void expect_delete(const int *keys, int n, int key, int spins_want, const char *expect) {
    RbTree *t = build(keys, n);
    int spins = 0;
    char *got;
    CHECK(rb_delete(t, key, NULL, &spins) == 0);
    CHECK_EQ(spins, spins_want);
    assert_rb(t);
    got = shape_of(t);
    CHECK_STR(got, expect);
    free(got);
    rb_free(t);
}

static void check_delete_hand(void) {
    static const int keys[] = {1, 2, 3, 4, 5, 6, 7};
    static const int small[] = {1, 2, 3};
    char *buf = NULL;
    size_t size = 0;
    FILE *fp;
    RbTree *t = build(keys, 7);
    int spins = 0;
    expect_delete(keys, 7, 7, 0, "2[B](1[B],4[R](3[B],6[B](5[R],#)))");
    expect_delete(keys, 7, 3, 1, "2[B](1[B],6[R](4[B](#,5[R]),7[B]))");
    expect_delete(keys, 7, 4, 0, "2[B](1[B],5[R](3[B],6[B](#,7[R])))");
    expect_delete(keys, 7, 1, 1, "4[B](2[B](#,3[R]),6[B](5[R],7[R]))");
    expect_delete(small, 3, 2, 0, "3[B](1[R],#)");

    fp = open_memstream(&buf, &size);
    CHECK(rb_delete(t, 1, fp, &spins) == 0);
    fclose(fp);
    CHECK(has_step(buf, "fix case=1 side=L sib=4\n", "absorb 2 B\n"));
    CHECK(strstr(buf, "fix case=2 side=L sib=3\n") != NULL);
    free(buf);
    rb_free(t);

    t = rb_create();
    CHECK(rb_insert(t, 1, NULL, NULL) == 0);
    CHECK(rb_delete(t, 1, NULL, &spins) == 0);
    CHECK(t->root == t->nil);
    CHECK(rb_delete(t, 1, NULL, &spins) == 1);
    rb_free(t);
}

static void perm_insert(int *a, int n, int i) {
    if (i == n) {
        RbTree *t = build(a, n);
        RbStats s;
        rb_stats(t, &s);
        CHECK_EQ(s.nodes, n);
        rb_free(t);
        return;
    }
    for (int j = i; j < n; j++) {
        int tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
        perm_insert(a, n, i + 1);
        tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
    }
}

static void delete_all(const int *keys, int n, const int *order) {
    RbTree *t = build(keys, n);
    for (int i = 0; i < n; i++) {
        int spins = 0;
        CHECK(rb_delete(t, order[i], NULL, &spins) == 0);
        CHECK(spins <= 3);
        assert_rb(t);
    }
    CHECK(t->root == t->nil);
    rb_free(t);
}

static void perm_delete(int *order, int n, int i, const int *keys) {
    if (i == n) {
        delete_all(keys, n, order);
        return;
    }
    for (int j = i; j < n; j++) {
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
        perm_delete(order, n, i + 1, keys);
        tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
}

static void check_perms(void) {
    int asc[6];
    int desc[6];
    int order[4];
    int keys[4];
    for (int n = 1; n <= 6; n++) {
        int a[6];
        for (int i = 0; i < n; i++) {
            a[i] = i + 1;
        }
        perm_insert(a, n, 0);
    }
    for (int i = 0; i < 6; i++) {
        asc[i] = i + 1;
        desc[i] = 6 - i;
    }
    delete_all(asc, 6, asc);
    delete_all(asc, 6, desc);
    for (int i = 0; i < 4; i++) {
        keys[i] = i + 1;
        order[i] = i + 1;
    }
    perm_insert(keys, 4, 0);
    for (int i = 0; i < 4; i++) {
        keys[i] = i + 1;
    }
    perm_delete(order, 4, 0, keys);
}

static void expect_stats(const char *shape, int rb, int bh_ok, int redred, int bst) {
    RbTree *t = rb_create();
    RbStats s;
    CHECK(rb_parse(t, shape) == 0);
    rb_stats(t, &s);
    CHECK_EQ(s.rb, rb);
    CHECK_EQ(s.bh_ok, bh_ok);
    CHECK_EQ(s.redred, redred);
    CHECK_EQ(s.bst, bst);
    if (strcmp(shape, "#") == 0) {
        CHECK_EQ(s.bh, 0);
        CHECK_EQ(s.root_state, 0);
    }
    if (strcmp(shape, "1[R]") == 0) {
        CHECK_EQ(s.root_state, 2);
    }
    if (strcmp(shape, "2[B](1[R](0[R],#),3[R])") == 0) {
        CHECK(s.has_pair);
        CHECK_EQ(s.pair_parent, 1);
        CHECK_EQ(s.pair_child, 0);
    }
    rb_free(t);
}

static void check_judge(void) {
    RbTree *t = rb_create();
    char *round;
    expect_stats("#", 1, 1, 1, 1);
    expect_stats("1[R]", 0, 1, 1, 1);
    expect_stats("2[B](1[R](0[R],#),3[R])", 0, 1, 0, 1);
    expect_stats("2[B](1[B],3[R])", 0, 0, 1, 1);
    expect_stats("2[B](3[B],4[B])", 0, 1, 1, 0);
    CHECK(rb_parse(t, "1[G]") != 0);
    CHECK(t->root == t->nil);
    CHECK(rb_parse(t, "2[B](1[R])") != 0);
    rb_free(t);

    t = build((const int[]){1, 2, 3, 4, 5, 6, 7}, 7);
    round = shape_of(t);
    {
        RbTree *parsed = rb_create();
        char *again;
        CHECK(rb_parse(parsed, round) == 0);
        again = shape_of(parsed);
        CHECK_STR(again, round);
        assert_rb(parsed);
        free(again);
        rb_free(parsed);
    }
    free(round);
    rb_free(t);
}

static void check_dup(void) {
    RbTree *t = rb_create();
    char *got;
    CHECK(rb_insert(t, 1, NULL, NULL) == 0);
    CHECK(rb_insert(t, 1, NULL, NULL) == 1);
    got = shape_of(t);
    CHECK_STR(got, "1[B]");
    free(got);
    rb_free(t);
}

int main(void) {
    check_kinds();
    check_sequence();
    check_delete_hand();
    check_perms();
    check_judge();
    check_dup();
    return TEST_SUMMARY();
}
