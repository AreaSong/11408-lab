#include "bp.h"
#include "bt.h"
#include "limit.h"

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *capture_bt(BtTree *tree) {
    char *buf = NULL;
    size_t n = 0;
    FILE *fp = open_memstream(&buf, &n);
    if (!fp) {
        exit(1);
    }
    bt_report(tree, fp);
    fclose(fp);
    return buf;
}

static void expect_keys(const BtTree *tree, const int *keys, int n) {
    int got[BT_MAX_N];
    int count = bt_collect(tree, got, BT_MAX_N);
    CHECK(bt_valid(tree));
    CHECK_EQ(count, n);
    for (int i = 0; i < n && count == n; i++) {
        CHECK_EQ(got[i], keys[i]);
    }
}

static BtTree *load_tree(int order, char **tok, int ntok) {
    BtTree *tree = bt_create(order);
    CHECK_EQ(bt_load(tree, tok, ntok), BT_OK);
    CHECK(bt_valid(tree));
    return tree;
}

static void test_bounds(void) {
    long long min_k = 0;
    long long max_k = 0;
    long long min_n = 0;
    long long max_n = 0;
    long long nodes = 0;
    long long shapes = 0;
    CHECK(limit_bounds(5, 2, &min_k, &max_k, &min_n, &max_n));
    CHECK_EQ(min_k, 5);
    CHECK_EQ(max_k, 24);
    CHECK_EQ(min_n, 3);
    CHECK_EQ(max_n, 6);
    CHECK(limit_bounds(3, 5, &min_k, &max_k, &min_n, &max_n));
    CHECK_EQ(min_k, 31);
    CHECK_EQ(max_k, 242);
    CHECK_EQ(min_n, 31);
    CHECK_EQ(max_n, 121);
    CHECK(limit_bounds(3, 1, &min_k, &max_k, &min_n, &max_n));
    CHECK_EQ(min_k, 1);
    CHECK_EQ(max_k, 2);
    CHECK(limit_max_nodes(15, 4, &nodes));
    CHECK_EQ(nodes, 15);
    CHECK(limit_max_nodes(4, 4, &nodes));
    CHECK_EQ(nodes, 3);
    CHECK(limit_max_nodes(2, 4, &nodes));
    CHECK_EQ(nodes, 1);
    CHECK(limit_max_nodes(5, 5, &nodes));
    CHECK_EQ(nodes, 3);
    CHECK(limit_most(3, 3, 4, &nodes));
    CHECK_EQ(nodes, 11);
    CHECK(limit_most(3, 2, 4, &nodes));
    CHECK_EQ(nodes, 4);
    CHECK(limit_shapes(7, 4, &shapes));
    CHECK_EQ(shapes, 9);
    CHECK(limit_shapes(1, 4, &shapes));
    CHECK_EQ(shapes, 1);
    CHECK(limit_shapes(4, 4, &shapes));
    CHECK_EQ(shapes, 2);
}

static void test_insert_shapes(void) {
    BtTree *tree = bt_create(3);
    char *text;
    int seq[] = {1, 2, 3};
    int exam[] = {5, 6, 9, 13, 8, 2, 12, 15};
    int expect[] = {2, 5, 6, 8, 9, 12, 13, 15};
    for (int i = 0; i < 3; i++) {
        CHECK_EQ(bt_insert(tree, seq[i], NULL), BT_OK);
        CHECK(bt_valid(tree));
    }
    text = capture_bt(tree);
    CHECK_STR(text, "tree (2 (1) (3))\nheight=2 keys=3 nodes=3\nroot 2\nterm 1 | 3\nchain=no\n");
    free(text);
    bt_free(tree);
    tree = bt_create(4);
    for (int i = 0; i < 8; i++) {
        CHECK_EQ(bt_insert(tree, exam[i], NULL), BT_OK);
        CHECK(bt_valid(tree));
    }
    CHECK_EQ(tree->root->n, 2);
    CHECK_EQ(tree->root->key[0], 6);
    CHECK_EQ(tree->root->key[1], 9);
    CHECK_EQ(bt_height(tree), 2);
    expect_keys(tree, expect, 8);
    text = capture_bt(tree);
    CHECK_STR(text,
              "tree (6 9 (2 5) (8) (12 13 15))\nheight=2 keys=8 nodes=4\nroot 6 9\n"
              "term 2 5 | 8 | 12 13 15\nchain=no\n");
    free(text);
    CHECK_EQ(bt_insert(tree, 8, NULL), BT_DUP);
    bt_free(tree);
}

static void delete_and_check(BtTree *tree, int key, const char *trace, const int *keys, int n) {
    char *buf = NULL;
    size_t size = 0;
    FILE *fp = open_memstream(&buf, &size);
    BtPolicy policy = {0, 0};
    if (!fp) {
        exit(1);
    }
    CHECK_EQ(bt_delete(tree, key, policy, fp), BT_OK);
    fclose(fp);
    CHECK_STR(buf, trace);
    free(buf);
    expect_keys(tree, keys, n);
    bt_free(tree);
}

static void test_delete(void) {
    char *borrow[] = {"(", "65", "(", "60", "62", ")", "(", "78", ")", ")"};
    char *direct[] = {"(", "62", "(", "60", ")", "(", "65", "78", ")", ")"};
    char *from_right[] = {"(", "20", "(", "10", ")", "(", "30", "40", ")", ")"};
    char *merge_r[] = {"(", "20", "(", "10", ")", "(", "30", ")", ")"};
    char *merge_l[] = {"(", "20", "(", "10", ")", "(", "30", ")", ")"};
    char *pred[] = {"(", "20", "(", "10", "15", ")", "(", "30", ")", ")"};
    int keys_a[] = {60, 62, 65};
    int keys_b[] = {20, 30, 40};
    int keys_c[] = {20, 30};
    int keys_d[] = {10, 20};
    int keys_e[] = {10, 15, 30};
    delete_and_check(load_tree(3, borrow, (int)(sizeof borrow / sizeof borrow[0])), 78,
                     "borrow left up=62 down=65\n", keys_a, 3);
    delete_and_check(load_tree(3, direct, (int)(sizeof direct / sizeof direct[0])), 78, "", keys_a, 3);
    delete_and_check(load_tree(3, from_right, (int)(sizeof from_right / sizeof from_right[0])), 10,
                     "borrow right up=30 down=20\n", keys_b, 3);
    delete_and_check(load_tree(3, merge_r, (int)(sizeof merge_r / sizeof merge_r[0])), 10,
                     "merge right down=20\nshrink\n", keys_c, 2);
    delete_and_check(load_tree(3, merge_l, (int)(sizeof merge_l / sizeof merge_l[0])), 30,
                     "merge left down=20\nshrink\n", keys_d, 2);
    delete_and_check(load_tree(3, pred, (int)(sizeof pred / sizeof pred[0])), 20, "pred 15\n", keys_e, 3);
}

static void test_policies(void) {
    char *tok[] = {"(", "60", "90", "260", "350", "(", "10", "20", ")", "(", "70", "80", "85", ")",
                   "(", "100", "110", ")", "(", "280", "300", ")", "(", "400", "500", ")", ")"};
    BtPolicy policies[3] = {{0, 0}, {1, 0}, {1, 1}};
    int root0[] = {60, 85, 110, 350};
    int root1[] = {60, 90, 350};
    int root2[] = {60, 90, 280};
    int *roots[] = {root0, root1, root2};
    int lens[] = {4, 3, 3};
    for (int p = 0; p < 3; p++) {
        BtTree *tree = load_tree(5, tok, (int)(sizeof tok / sizeof tok[0]));
        CHECK_EQ(bt_delete(tree, 260, policies[p], NULL), BT_OK);
        CHECK(bt_valid(tree));
        CHECK_EQ(tree->root->n, lens[p]);
        for (int i = 0; i < lens[p]; i++) {
            CHECK_EQ(tree->root->key[i], roots[p][i]);
        }
        bt_free(tree);
    }
}

static void test_stress(void) {
    int orders[] = {3, 4, 5};
    for (int oi = 0; oi < 3; oi++) {
        for (int pass = 0; pass < 2; pass++) {
            BtTree *tree = bt_create(orders[oi]);
            BtPolicy policy = {0, 0};
            for (int key = 1; key <= 40; key++) {
                CHECK_EQ(bt_insert(tree, key, NULL), BT_OK);
                CHECK(bt_valid(tree));
            }
            for (int step = 0; step < 40; step++) {
                int key = pass == 0 ? step + 1 : 40 - step;
                int left = 40 - step - 1;
                CHECK_EQ(bt_delete(tree, key, policy, NULL), BT_OK);
                CHECK(bt_valid(tree));
                CHECK_EQ(tree->key_count, left);
            }
            CHECK(tree->root == NULL);
            bt_free(tree);
        }
    }
}

static void test_bplus(void) {
    int exam[] = {5, 6, 9, 13, 8, 2, 12, 15};
    int expect[] = {2, 5, 6, 8, 9, 12, 13, 15};
    int got[16];
    BpTree *tree = bp_create(4);
    char *buf = NULL;
    size_t n = 0;
    FILE *fp = open_memstream(&buf, &n);
    for (int i = 0; i < 8; i++) {
        CHECK_EQ(bp_insert(tree, exam[i], NULL), BP_OK);
        CHECK(bp_valid(tree));
    }
    CHECK_EQ(bp_leaves(tree, got, 16), 8);
    for (int i = 0; i < 8; i++) {
        CHECK_EQ(got[i], expect[i]);
    }
    CHECK_EQ(tree->root->n, 3);
    CHECK_EQ(tree->root->key[0], 6);
    CHECK_EQ(tree->root->key[1], 9);
    CHECK_EQ(tree->root->key[2], 15);
    bp_report(tree, fp);
    bp_find(tree, 9, fp);
    bp_find(tree, 6, fp);
    fclose(fp);
    CHECK(strstr(buf, "chain=yes") != NULL);
    CHECK(strstr(buf, "leaf 2 5 6 | 8 9 | 12 13 15") != NULL);
    CHECK(strstr(buf, "9 = copy") != NULL);
    CHECK(strstr(buf, "hit leaf=yes") != NULL);
    free(buf);
    bp_free(tree);
    for (int order = 3; order <= 5; order++) {
        tree = bp_create(order);
        for (int key = 1; key <= 40; key++) {
            CHECK_EQ(bp_insert(tree, key, NULL), BP_OK);
            CHECK(bp_valid(tree));
            CHECK_EQ(bp_find(tree, key, NULL), BP_OK);
        }
        CHECK_EQ(bp_find(tree, 41, NULL), BP_MISS);
        bp_free(tree);
    }
}

static void test_rise(void) {
    BtTree *tree = bt_create(5);
    char *buf = NULL;
    size_t n = 0;
    FILE *fp = open_memstream(&buf, &n);
    int keys[] = {10, 20, 30, 40, 25};
    for (int i = 0; i < 5; i++) {
        CHECK_EQ(bt_insert(tree, keys[i], NULL), BT_OK);
    }
    bt_find(tree, 25, fp);
    fclose(fp);
    CHECK(strstr(buf, "hit term=no") != NULL);
    CHECK_EQ(tree->root->key[0], 25);
    free(buf);
    bt_free(tree);
}

static void test_bad_load(void) {
    char *tok[] = {"(", "10", "(", "20", ")", "(", "5", ")", ")"};
    BtTree *tree = bt_create(3);
    CHECK_EQ(bt_load(tree, tok, (int)(sizeof tok / sizeof tok[0])), BT_BAD);
    CHECK(tree->root == NULL);
    bt_free(tree);
}

int main(void) {
    test_bounds();
    test_insert_shapes();
    test_delete();
    test_policies();
    test_stress();
    test_bplus();
    test_rise();
    test_bad_load();
    return TEST_SUMMARY();
}
