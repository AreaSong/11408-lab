#include "avl.h"
#include "bst.h"
#include "exam.h"
#include "node.h"

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *capture_exam(const char *id) {
    char *buf = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&buf, &size);
    exam_run(id, out);
    fclose(out);
    return buf;
}

static void check_pick(const char *id, char pick) {
    char *buf = capture_exam(id);
    char line[16];
    snprintf(line, sizeof line, "pick=%c\n", pick);
    CHECK(buf != NULL);
    CHECK(strstr(buf, line) != NULL);
    CHECK(strstr(buf, "pick=?") == NULL);
    free(buf);
}

static Node *build_bst(const int *keys, int n) {
    Node *t = NULL;
    for (int i = 0; i < n; i++) {
        int dup = 0;
        t = bst_insert(t, keys[i], &dup);
        CHECK(!dup);
    }
    return t;
}

static Node *build_avl(const int *keys, int n) {
    Node *t = NULL;
    for (int i = 0; i < n; i++) {
        int dup = 0;
        t = avl_insert(t, keys[i], 0, NULL, &dup);
        CHECK(!dup);
        CHECK(node_bst_ok(t));
        CHECK(node_height_ok(t));
    }
    return t;
}

static void check_shape(const Node *t, int with_bf, const char *expect) {
    char buf[256];
    CHECK(node_shape(t, buf, (int)sizeof buf, with_bf) >= 0);
    CHECK_STR(buf, expect);
}

static void check_rotations(void) {
    static const int ll[] = {30, 20, 10};
    static const int rr[] = {10, 20, 30};
    static const int lr[] = {30, 10, 20};
    static const int rl[] = {10, 30, 20};
    Node *t = build_avl(ll, 3);
    check_shape(t, 1, "20[0](10[0],30[0])");
    node_free(t);
    t = build_avl(rr, 3);
    check_shape(t, 1, "20[0](10[0],30[0])");
    node_free(t);
    t = build_avl(lr, 3);
    check_shape(t, 1, "20[0](10[0],30[0])");
    node_free(t);
    t = build_avl(rl, 3);
    check_shape(t, 1, "20[0](10[0],30[0])");
    node_free(t);
}

static void check_delete_cases(void) {
    static const int keys[] = {8, 3, 10, 1, 6, 14, 4, 7, 13};
    int found = 0;
    Node *t = build_bst(keys, 9);
    check_shape(t, 0, "8(3(1,6(4,7)),10(#,14(13,#)))");
    t = bst_delete(t, 4, NULL, &found);
    CHECK(found);
    check_shape(t, 0, "8(3(1,6(#,7)),10(#,14(13,#)))");
    node_free(t);

    t = build_bst(keys, 9);
    found = 0;
    t = bst_delete(t, 14, NULL, &found);
    CHECK(found);
    check_shape(t, 0, "8(3(1,6(4,7)),10(#,13))");
    node_free(t);

    t = build_bst(keys, 9);
    found = 0;
    t = bst_delete(t, 3, NULL, &found);
    CHECK(found);
    check_shape(t, 0, "8(1(#,6(4,7)),10(#,14(13,#)))");
    node_free(t);
}

static void check_pred_with_left(void) {
    static const int keys[] = {10, 6, 12, 8, 7};
    int found = 0;
    Node *t = build_bst(keys, 5);
    /* 10 的前驱是 8，8 还有左孩子 7。摘掉 8 时要把 7 接回 6 的右边。 */
    check_shape(t, 0, "10(6(#,8(7,#)),12)");
    t = bst_delete(t, 10, NULL, &found);
    CHECK(found);
    check_shape(t, 0, "8(6(#,7),12)");
    node_free(t);
}

static void check_asl_numbers(void) {
    static const int keys[] = {45, 24, 53, 12, 37, 93};
    static const int wkeys[] = {2, 1, 4, 3};
    static const int w[] = {15, 35, 35, 15};
    Node *t = build_bst(keys, 6);
    Asl asl;
    long long num = 0;
    long long den = 0;
    bst_asl(t, &asl);
    CHECK_EQ(asl.succ_num, 14);
    CHECK_EQ(asl.succ_den, 6);
    CHECK_EQ(asl.fail_num, 20);
    CHECK_EQ(asl.fail_den, 7);
    node_free(t);
    t = build_bst(wkeys, 4);
    bst_asl_weight(t, wkeys, w, 4, &num, &den);
    CHECK_EQ(num, 200);
    CHECK_EQ(den, 100);
    node_free(t);
}

static void check_roundtrip(void) {
    static const int keys[] = {5, 3, 7, 2, 4, 6};
    int found = 0;
    int dup = 0;
    Node *t = build_bst(keys, 6);
    Node *orig = node_clone(t);
    t = bst_delete(t, 2, NULL, &found);
    t = bst_insert(t, 2, &dup);
    CHECK(node_equal(orig, t));
    node_free(t);
    node_free(orig);

    t = build_bst(keys, 6);
    orig = node_clone(t);
    found = 0;
    t = bst_delete(t, 7, NULL, &found);
    t = bst_insert(t, 7, &dup);
    CHECK(!node_equal(orig, t));
    node_free(t);
    node_free(orig);

    t = build_bst(keys, 6);
    orig = node_clone(t);
    found = 0;
    t = bst_delete(t, 3, NULL, &found);
    t = bst_insert(t, 3, &dup);
    CHECK(!node_equal(orig, t));
    node_free(t);
    node_free(orig);
}

static int nonleaf_bf1(const Node *t, int *nonleaf) {
    int good;
    if (!t || (!t->left && !t->right)) {
        return 0;
    }
    good = node_bf(t) == 1;
    (*nonleaf)++;
    return good + nonleaf_bf1(t->left, nonleaf) + nonleaf_bf1(t->right, nonleaf);
}

static void check_min_nodes(void) {
    static const int expect[] = {0, 1, 2, 4, 7, 12, 20, 33};
    for (int h = 0; h <= 7; h++) {
        CHECK_EQ(avl_min_count(h), expect[h]);
    }
    for (int h = 1; h <= 7; h++) {
        Node *t = avl_build_min(h);
        int nonleaf = 0;
        int good = nonleaf_bf1(t, &nonleaf);
        CHECK_EQ(node_count(t), expect[h]);
        CHECK_EQ(node_height(t), h);
        CHECK(node_bst_ok(t));
        CHECK(node_height_ok(t));
        CHECK_EQ(good, nonleaf);
        node_free(t);
    }
}

static void check_bf0_delete(void) {
    static const int keys[] = {4, 2, 5, 1, 3};
    char *buf = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&buf, &size);
    int found = 0;
    Node *t = build_avl(keys, 5);
    t = avl_delete(t, 5, out, &found);
    fclose(out);
    CHECK(found);
    CHECK(node_height_ok(t));
    CHECK(strstr(buf, "childbf=0 kind=LL\n") != NULL);
    check_shape(t, 1, "2[-1](1[0],4[1](3[0],#))");
    node_free(t);
    free(buf);
}

static void check_desc_order(void) {
    static const int keys[] = {3, 1, 2};
    Node *t = NULL;
    char buf[64];
    for (int i = 0; i < 3; i++) {
        int dup = 0;
        t = avl_insert(t, keys[i], 1, NULL, &dup);
        CHECK(node_height_ok(t));
    }
    {
        char *mem = NULL;
        size_t size = 0;
        FILE *out = open_memstream(&mem, &size);
        node_inorder(t, out);
        fclose(out);
        snprintf(buf, sizeof buf, "%s", mem);
        free(mem);
    }
    CHECK_STR(buf, "3 2 1");
    node_free(t);
}

static int pivot_lines(const char *s) {
    int n = 0;
    for (const char *p = s; (p = strstr(p, "\npivot ")) != NULL; p += 7) {
        n++;
    }
    return n;
}

static void check_delete_cascade(void) {
    int found_multi = 0;
    for (int h = 4; h <= 6 && !found_multi; h++) {
        Node *base = avl_build_min(h);
        int n = node_count(base);
        for (int key = 1; key <= n && !found_multi; key++) {
            Node *t = node_clone(base);
            char *buf = NULL;
            size_t size = 0;
            FILE *out = open_memstream(&buf, &size);
            int found = 0;
            t = avl_delete(t, key, out, &found);
            fclose(out);
            CHECK(found);
            CHECK(node_bst_ok(t));
            CHECK(node_height_ok(t));
            if (buf[0] != '\0' && pivot_lines(buf) >= 2) {
                found_multi = 1;
            }
            node_free(t);
            free(buf);
        }
        node_free(base);
    }
    CHECK(found_multi);
}

static void check_2010_children(void) {
    static const int keys[] = {24, 13, 53, 37, 90, 48};
    Node *t = build_avl(keys, 6);
    const Node *p = node_find(t, 37);
    CHECK(p != NULL && p->left && p->right);
    CHECK_EQ(p->left->key, 24);
    CHECK_EQ(p->right->key, 53);
    node_free(t);
}

int main(void) {
    check_rotations();
    check_delete_cases();
    check_pred_with_left();
    check_asl_numbers();
    check_roundtrip();
    check_min_nodes();
    check_bf0_delete();
    check_desc_order();
    check_delete_cascade();
    check_2010_children();
    check_pick("2010-4", 'C');
    check_pick("2012-4", 'B');
    check_pick("2013-3", 'D');
    check_pick("2013-6", 'C');
    check_pick("2015-4", 'D');
    check_pick("2019-4", 'A');
    check_pick("2020-5", 'B');
    check_pick("2024-7", 'D');
    {
        char *buf = capture_exam("2013-42");
        CHECK(strstr(buf, "half=220/100 dec=2.20\n") != NULL);
        CHECK(strstr(buf, "seq=210/100 dec=2.10\n") != NULL);
        CHECK(strstr(buf, "for_root=200/100 dec=2.00\n") != NULL);
        CHECK(strstr(buf, "repeat_root=200/100 dec=2.00\n") != NULL);
        free(buf);
    }
    return TEST_SUMMARY();
}
