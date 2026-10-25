#include "build.h"
#include "expr.h"
#include "prop.h"
#include "seq.h"
#include "test.h"
#include "tree.h"
#include "walk.h"
#include "wpl.h"

#include <string.h>

static void check_orders(const char *s, const char *pre, const char *inord, const char *post, const char *level,
                         int width, int complete) {
    BiTree tree;
    char buf[BT_MAX];
    char got[BT_MAX];
    int w = 0;
    CHECK(tree_from_pre(&tree, s) == 0);
    CHECK(tree_pre(&tree, tree.root, buf, BT_MAX) == 0);
    CHECK_STR(buf, pre);
    CHECK(walk_pre(&tree, got, BT_MAX, NULL) == 0);
    CHECK_STR(got, pre);
    CHECK(tree_in(&tree, tree.root, buf, BT_MAX) == 0);
    CHECK_STR(buf, inord);
    CHECK(walk_in(&tree, got, BT_MAX, NULL) == 0);
    CHECK_STR(got, inord);
    CHECK(tree_post(&tree, tree.root, buf, BT_MAX) == 0);
    CHECK_STR(buf, post);
    CHECK(walk_post(&tree, got, BT_MAX, NULL) == 0);
    CHECK_STR(got, post);
    CHECK(walk_level(&tree, got, BT_MAX, &w, NULL) == 0);
    CHECK_STR(got, level);
    CHECK_EQ(w, width);
    CHECK_EQ(walk_complete(&tree, NULL), complete);
}

static void check_formula(const BiTree *tree) {
    TreeInfo info;
    char pre[BT_MAX];
    char inord[BT_MAX];
    char post[BT_MAX];
    CHECK(tree_info(tree, &info, pre, inord, post, BT_MAX) == 0);
    CHECK_EQ(info.n0, info.n2 + 1);
    CHECK_EQ(info.nulls, info.n + 1);
    CHECK_EQ(info.n, info.n0 + info.n1 + info.n2);
    CHECK(info.height >= 1);
}

static unsigned rnd(unsigned *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int grow(BiTree *tree, unsigned *state, int budget) {
    if (budget <= 0 || (tree->n > 0 && rnd(state) % 5u == 0)) {
        return 0;
    }
    int id = tree_alloc(tree, 'A' + (tree->n % 26));
    if (id == 0) {
        return 0;
    }
    int left_budget = (int)(rnd(state) % (unsigned)budget);
    tree->node[id].left = grow(tree, state, left_budget);
    tree->node[id].right = grow(tree, state, budget - 1 - left_budget);
    return id;
}

static void make_cbt(BiTree *tree, int n) {
    tree_init(tree);
    for (int i = 0; i < n; i++) {
        CHECK(tree_alloc(tree, 'A' + (i % 26)) != 0);
    }
    tree->root = 1;
    for (int i = 1; i <= n; i++) {
        tree->node[i].left = (i * 2 <= n) ? i * 2 : 0;
        tree->node[i].right = (i * 2 + 1 <= n) ? i * 2 + 1 : 0;
    }
}

static void chain_in(const char *s, int i, int n, unsigned mask, char *out, int *k) {
    if (i == n) {
        return;
    }
    int has_child = i + 1 < n;
    int on_left = has_child && (mask & (1u << i)) != 0;
    if (on_left) {
        chain_in(s, i + 1, n, mask, out, k);
    }
    out[(*k)++] = s[i];
    if (has_child && !on_left) {
        chain_in(s, i + 1, n, mask, out, k);
    }
}

static int form_has(const BtForm *forms, int n, const char *inord) {
    for (int i = 0; i < n; i++) {
        if (strcmp(forms[i].inord, inord) == 0) {
            return 1;
        }
    }
    return 0;
}

static void check_wpl(char **argv, int argc, int expect) {
    int sum = -1;
    CHECK(wpl_eval(argc, argv, &sum, NULL) == 0);
    CHECK_EQ(sum, expect);
}

static void check_expr(char **argv, int argc, const char *expect) {
    char buf[128];
    CHECK(expr_eval(argc, argv, buf, (int)sizeof buf, NULL) == 0);
    CHECK_STR(buf, expect);
}

static void check_known(void) {
    check_orders("ABD##E##C##", "ABDEC", "DBEAC", "DEBCA", "ABCDE", 2, 1);
    check_orders("AB##C##", "ABC", "BAC", "BCA", "ABC", 2, 1);
    check_orders("A##", "A", "A", "A", "A", 1, 1);
    check_orders("A#B#C##", "ABC", "ABC", "CBA", "ABC", 1, 0);
    check_orders("ABC####", "ABC", "CBA", "CBA", "ABC", 1, 0);
    check_orders("ABD##E##C#F##", "ABDECF", "DBEACF", "DEBFCA", "ABCDEF", 3, 0);
    check_orders("AB#E##C##", "ABEC", "BEAC", "EBCA", "ABCE", 2, 0);
}

static void check_flags(void) {
    BiTree tree;
    TreeInfo info;
    char pre[BT_MAX];
    char inord[BT_MAX];
    char post[BT_MAX];
    CHECK(tree_from_pre(&tree, "ABD##E##C##") == 0);
    check_formula(&tree);
    tree_info(&tree, &info, pre, inord, post, BT_MAX);
    CHECK_EQ(info.n0, 3);
    CHECK_EQ(info.n1, 0);
    CHECK_EQ(info.n2, 2);
    CHECK_EQ(info.nulls, 6);
    CHECK_EQ(info.height, 3);
    CHECK_EQ(info.pre_eq_in, 0);
    CHECK(tree_from_pre(&tree, "A#B#C##") == 0);
    tree_info(&tree, &info, pre, inord, post, BT_MAX);
    CHECK_EQ(info.pre_eq_in, 1);
    CHECK_EQ(info.no_left, 1);
    CHECK_EQ(info.nonleaf_deg1, 1);
    CHECK_EQ(info.all_deg1, 0);
    CHECK(tree_from_pre(&tree, "ABC####") == 0);
    tree_info(&tree, &info, pre, inord, post, BT_MAX);
    CHECK_EQ(info.pre_eq_in, 0);
    CHECK_EQ(info.no_right, 1);
    CHECK_EQ(info.nonleaf_deg2, 0);
}

static void check_random(void) {
    unsigned state = 20261025u;
    for (int trial = 0; trial < 30; trial++) {
        BiTree tree;
        tree_init(&tree);
        CHECK(grow(&tree, &state, 8) != 0);
        tree.root = 1;
        check_formula(&tree);
    }
}

static void check_one_cbt(int n) {
    BiTree tree;
    CbtStat stat;
    TreeInfo info;
    char pre[BT_MAX];
    char inord[BT_MAX];
    char post[BT_MAX];
    make_cbt(&tree, n);
    CHECK_EQ(walk_complete(&tree, NULL), 1);
    CHECK(cbt_stat(n, &stat) == 0);
    check_formula(&tree);
    tree_info(&tree, &info, pre, inord, post, BT_MAX);
    CHECK_EQ(info.n0, stat.n0);
    CHECK_EQ(info.height, stat.height);
    CHECK_EQ(stat.leaves, stat.n0);
    if (stat.has_layers) {
        CHECK_EQ(stat.leaf_sum, stat.n0);
    }
}

static void check_cbt_index(int n) {
    for (int i = 1; i <= n; i++) {
        CbtAt at;
        CHECK(cbt_at(n, i, &at) == 0);
        CHECK_EQ(at.parent, i == 1 ? 0 : i / 2);
        CHECK_EQ(at.left, i * 2 <= n ? i * 2 : 0);
        CHECK_EQ(at.right, i * 2 + 1 <= n ? i * 2 + 1 : 0);
        CHECK_EQ(at.level, prop_floor_log2(i) + 1);
    }
}

static void check_numbers(void) {
    CbtStat big;
    CbtAt at;
    MostOut most;
    PropOut solved;
    BiTree tree;
    CHECK(cbt_stat(768, &big) == 0);
    CHECK_EQ(big.n0, 384);
    CHECK_EQ(big.n1, 1);
    CHECK_EQ(big.n2, 383);
    CHECK_EQ(big.height, 10);
    CHECK_EQ(big.bottom, 257);
    CHECK_EQ(big.leaf_sum, 384);
    CHECK_EQ(prop_height_min(10), 4);
    CHECK(cbt_at(10, 6, &at) == 0);
    CHECK_EQ(at.parent, 3);
    CHECK_EQ(at.side, 1);
    CHECK_EQ(at.sibling, 7);
    CHECK_EQ(at.i0, 5);
    CHECK_EQ(at.parent0, 2);
    CHECK_EQ(at.left0, -1);
    CHECK(most_compute(6, 8, &most) == 0);
    CHECK_EQ(most.min_nodes, 39);
    CHECK_EQ(most.max_nodes, 111);
    CHECK_EQ(most.branch, 24);
    CHECK_EQ(most.child_max, 48);
    CHECK(prop_solve(3, -1, 0, -1, &solved) == 0);
    CHECK_EQ(solved.n0, 2);
    CHECK_EQ(solved.n2, 1);
    CHECK(prop_solve(2, -1, 0, -1, &solved) == 0);
    CHECK_STR(solved.reason, "parity");
    CHECK(prop_solve(768, -1, 1, -1, &solved) == 0);
    CHECK_EQ(solved.n0, 384);
    CHECK(prop_solve(-1, 2, -1, 0, &solved) == 0);
    CHECK_STR(solved.reason, "n0=n2+1");
    make_cbt(&tree, 5);
    tree.node[2].left = 0;
    CHECK_EQ(walk_complete(&tree, NULL), 0);
}

static void check_rebuild(void) {
    BiTree tree;
    BiTree back;
    SeqTree seq;
    char pre[BT_MAX];
    char post[BT_MAX];
    BtForm forms[PREPOST_MAX];
    char *hole[] = {"A", "#", "#", "B"};
    CHECK(build_pre_in(&tree, "ABDECF", "DBEACF", NULL) > 0);
    tree_post(&tree, tree.root, post, BT_MAX);
    CHECK_STR(post, "DEBFCA");
    CHECK(build_post_in(&tree, "DEBFCA", "DBEACF", NULL) > 0);
    tree_pre(&tree, tree.root, pre, BT_MAX);
    CHECK_STR(pre, "ABDECF");
    int nforms = prepost_forms("AB", "BA", forms, PREPOST_MAX);
    CHECK_EQ(nforms, 2);
    CHECK(form_has(forms, nforms, "AB"));
    CHECK(form_has(forms, nforms, "BA"));
    CHECK_EQ(prepost_forms("ABC", "BCA", forms, PREPOST_MAX), 1);
    CHECK_STR(forms[0].shape, "A(B,C)");
    CHECK_EQ(prepost_forms("ABC", "ACB", forms, PREPOST_MAX), 0);
    nforms = prepost_forms("1234", "4321", forms, PREPOST_MAX);
    CHECK_EQ(nforms, 8);
    CHECK_EQ(form_has(forms, nforms, "3241"), 0);
    for (unsigned mask = 0; mask < 8u; mask++) {
        char got[8];
        int k = 0;
        chain_in("1234", 0, 4, mask, got, &k);
        got[k] = '\0';
        CHECK(form_has(forms, nforms, got));
    }
    CHECK_EQ(prepost_forms("123456", "654321", forms, PREPOST_MAX), 32);
    CHECK(tree_from_pre(&tree, "A#B#C##") == 0);
    CHECK(seq_from_tree(&tree, &seq) == 0);
    CHECK_EQ(seq.last, 7);
    CHECK(seq.used[1] && seq.used[3] && seq.used[7] && !seq.used[2]);
    CHECK(seq_to_tree(&seq, &back) == 0);
    tree_pre(&back, back.root, pre, BT_MAX);
    CHECK_STR(pre, "ABC");
    CHECK(seq_from_tokens(&seq, hole, 4) != 0);
}

static void check_algo(void) {
    char *sample[] = {"4", "2", "5", "#", "#", "1", "#", "#", "3", "#", "#"};
    char *single[] = {"7", "#", "#"};
    char *inner[] = {"100", "2", "5", "#", "#", "1", "#", "#", "3", "#", "#"};
    char *degree1[] = {"9", "4", "#", "#", "#"};
    char *first[] = {"*", "+", "a", "#", "#", "b", "#", "#", "*", "c", "#", "#", "-", "#", "d", "#", "#"};
    char *second[] = {"+", "*", "a", "#", "#", "b", "#", "#", "-", "#", "-", "c", "#", "#", "d", "#", "#"};
    check_wpl(sample, 11, 15);
    check_wpl(single, 3, 0);
    check_wpl(inner, 11, 15);
    check_wpl(degree1, 5, 4);
    check_expr(first, 17, "(a+b)*(c*(-d))");
    check_expr(second, 17, "(a*b)+(-(c-d))");
}

int main(void) {
    check_known();
    check_flags();
    check_random();
    for (int n = 1; n <= 31; n++) {
        check_one_cbt(n);
        check_cbt_index(n);
    }
    check_numbers();
    check_rebuild();
    check_algo();
    return TEST_SUMMARY();
}
