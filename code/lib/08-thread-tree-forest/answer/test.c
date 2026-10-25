#include "forest.h"
#include "formula.h"
#include "thr.h"

#include "test.h"

#include <string.h>

static void check_in_threads(const char *pre, char data, char pred, char succ, int ltag, int rtag) {
    ThrTree tree;
    int id;
    int p;
    int s;
    CHECK_EQ(thr_from_pre(&tree, pre), THR_OK);
    CHECK_EQ(thr_thread_in(&tree), THR_OK);
    id = thr_find(&tree, data);
    CHECK(id != 0);
    p = thr_pred(&tree, id);
    s = thr_succ(&tree, id);
    if (pred == '^') {
        CHECK_EQ(p, tree.head);
    } else {
        CHECK_EQ(tree.node[p].data, pred);
    }
    if (succ == '^') {
        CHECK_EQ(s, tree.head);
    } else {
        CHECK_EQ(tree.node[s].data, succ);
    }
    CHECK_EQ(tree.node[id].ltag, ltag);
    CHECK_EQ(tree.node[id].rtag, rtag);
}

static void check_walk(const char *pre, const char *inord) {
    ThrTree tree;
    char got[THR_MAX];
    char walked[THR_MAX];
    int seq_n;
    CHECK_EQ(thr_from_pre(&tree, pre), THR_OK);
    CHECK_EQ(thr_inorder(&tree, got, THR_MAX), THR_OK);
    CHECK_STR(got, inord);
    CHECK_EQ(thr_sibling_adj(&tree), 0);
    CHECK_EQ(thr_two_child_ok(&tree), 1);
    CHECK_EQ(thr_thread_in(&tree), THR_OK);
    walked[0] = '\0';
    seq_n = 0;
    if (tree.root != 0) {
        int p = tree.node[tree.head].left;
        while (p != tree.head) {
            while (tree.node[p].ltag == 0) {
                p = tree.node[p].left;
            }
            walked[seq_n++] = tree.node[p].data;
            while (tree.node[p].rtag == 1 && tree.node[p].right != tree.head) {
                p = tree.node[p].right;
                walked[seq_n++] = tree.node[p].data;
            }
            p = tree.node[p].right;
        }
        walked[seq_n] = '\0';
    }
    CHECK_STR(walked, inord);
}

static void check_forest(const char *spec, const char *pre, const char *inord, int trees) {
    Forest forest;
    char got_pre[FOR_MAX];
    char got_in[FOR_MAX];
    char got_post[FOR_MAX];
    int leaves;
    CHECK_EQ(forest_parse(&forest, spec), THR_OK);
    CHECK_EQ(forest_seq(&forest, got_pre, got_in, got_post, FOR_MAX), THR_OK);
    CHECK_STR(got_pre, pre);
    CHECK_STR(got_in, inord);
    CHECK_EQ(forest.ntree, trees);
    leaves = forest_leaves(&forest);
    CHECK_EQ(forest_right_null(&forest), forest.n - leaves + 1);
    CHECK_EQ(forest_leaves(&forest), leaves);
    if (trees == 1) {
        CHECK_EQ(forest.node[forest.root[1]].next, 0);
    }
}

static int grand_iii(const char *spec) {
    Forest forest;
    GrandStats stats;
    CHECK_EQ(forest_parse(&forest, spec), THR_OK);
    forest_grand_stats(&forest, &stats);
    CHECK(stats.as_parent + stats.as_sibling + stats.as_uncle + stats.as_other == stats.total);
    return stats.as_iii;
}

static void check_built(void) {
    ThrTree tree;
    Forest forest;
    char right = 0;

    CHECK_EQ(thr_from_pre(&tree, "ayb##c##x##"), THR_OK);
    CHECK_EQ(thr_post_right(&tree, 'x', &right), THR_OK);
    CHECK_EQ(right, 'a');
    CHECK_EQ(thr_from_pre(&tree, "ab##c##"), THR_OK);
    CHECK_EQ(thr_post_right(&tree, 'b', &right), THR_OK);
    CHECK_EQ(right, 'c');
    CHECK_EQ(thr_from_pre(&tree, "ab##c##"), THR_OK);
    CHECK_EQ(thr_post_right(&tree, 'c', &right), THR_OK);
    CHECK_EQ(right, 'a');

    CHECK_EQ(thr_from_pre(&tree, "AB#"), THR_SHORT);
    CHECK_EQ(thr_from_pre(&tree, "AB##C##X"), THR_EXTRA);
    CHECK_EQ(thr_from_pre(&tree, ""), THR_EMPTY);
    CHECK_EQ(thr_from_pre(&tree, "AA###"), THR_DUP);

    check_forest("A(B(D,E),C) F(G)", "ABDECFG", "DEBCAGF", 2);
    check_forest("A(B(E,F,G),C,D)", "ABEFGCD", "EFGBCDA", 1);
    check_forest("A", "A", "A", 1);
    check_forest("A(B,C,F) D", "ABCFD", "BCFAD", 2);
    CHECK_EQ(forest_parse(&forest, "A(B,C,F) D"), THR_OK);
    CHECK_EQ(forest_leaves(&forest), 4);
    CHECK_EQ(forest_bin_leaf(&forest), 2);
    CHECK_EQ(forest_deg1(&forest), 2);
    CHECK_EQ(forest_right_null(&forest), 2);
}

static void check_orders(void) {
    Forest forest;
    char pre[FOR_MAX];
    char inord[FOR_MAX];
    char post[FOR_MAX];
    long long full = 0;

    CHECK_EQ(forest_from_bi(&forest, "abcdef", "badfec"), THR_OK);
    CHECK_EQ(forest_seq(&forest, pre, inord, post, FOR_MAX), THR_OK);
    CHECK_STR(post, "bfedca");
    CHECK_EQ(forest.ntree, 2);
    CHECK_STR(pre, "abcdef");
    CHECK_STR(inord, "badfec");

    CHECK_EQ(forest_from_bi(&forest, "abdcegf", "bdaegcf"), THR_OK);
    CHECK_EQ(forest_seq(&forest, pre, inord, post, FOR_MAX), THR_OK);
    CHECK_STR(post, "dbgefca");
    CHECK_EQ(forest.ntree, 3);
    CHECK_STR(forest.node[forest.root[1]].data == 'a' ? "a" : "?", "a");
    CHECK_EQ(forest.node[forest.node[forest.root[1]].next].data, 'c');
    CHECK_EQ(forest.node[forest.node[forest.node[forest.root[1]].next].next].data, 'f');

    CHECK_EQ(forest_parse(&forest, "A("), THR_FORMAT);
    CHECK_EQ(forest_parse(&forest, "A(B,B)"), THR_DUP);
    CHECK_EQ(forest_parse(&forest, ""), THR_EMPTY);
    CHECK_EQ(forest_from_bi(&forest, "ab", "ac"), THR_MISMATCH);
    CHECK_EQ(grand_iii("A(B,C) D(E,F,G) H(I,J(K)) L(M(N),O(P)) Q R S"), 0);
    CHECK_EQ(grand_iii("A(B,C(D),E) F(G,H(I))"), 0);

    CHECK_EQ(k_ary_max(3, 5, &full), THR_OK);
    CHECK_EQ(full, 121);
    CHECK_EQ(k_ary_max(3, 6, &full), THR_OK);
    CHECK_EQ(full, 364);
    CHECK_EQ(k_ary_max(1, 8, &full), THR_OK);
    CHECK_EQ(full, 8);
    CHECK_EQ(1 + 3 * 20 + 2 * 10 + 1 * 1, 82);
}

int main(void) {
    check_in_threads("abde###x##c##", 'x', 'b', 'a', 1, 1);
    check_in_threads("abde###x##c##", 'e', '^', 'd', 1, 1);
    check_in_threads("abde###x##c##", 'b', 'd', 'x', 0, 0);
    check_in_threads("ABD##E##C##", 'B', 'D', 'E', 0, 0);
    check_in_threads("A##", 'A', '^', '^', 1, 1);
    check_walk("abde###x##c##", "edbxac");
    check_walk("ABD##E##C##", "DBEAC");
    check_walk("AB##C##", "BAC");
    check_walk("A#B#C##", "ABC");
    check_walk("#", "");
    check_built();
    check_orders();
    return TEST_SUMMARY();
}
