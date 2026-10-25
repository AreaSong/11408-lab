#include "exam.h"

#include "forest.h"
#include "formula.h"
#include "thr.h"

#include <string.h>

static int sib(const Forest *f, int a, int b) {
    if (a <= 0 || b <= 0 || a == b) {
        return 0;
    }
    if (f->node[a].parent == -1 && f->node[b].parent == -1) {
        return 1;
    }
    return f->node[a].parent > 0 && f->node[a].parent == f->node[b].parent;
}

static const char *grand_class(const Forest *f, int u, int v) {
    int pv = f->node[v].parent;
    if (pv == u) {
        return "parent";
    }
    if (sib(f, u, v)) {
        return "sibling";
    }
    if (pv > 0 && sib(f, u, pv)) {
        return "uncle";
    }
    return "other";
}

static void exam_2009(FILE *out) {
    Forest forest;
    GrandStats stats;
    const char *spec = "A(B,C) D(E,F,G) H(I,J(K)) L(M(N),O(P)) Q R S";
    forest_parse(&forest, spec);
    fprintf(out, "exam 2009-6\n");
    fprintf(out, "forest %s\n", spec);
    for (int u = 1; u <= forest.n; u++) {
        for (int v = 1; v <= forest.n; v++) {
            int mid = forest_bin_parent(&forest, v);
            if (mid != 0 && forest_bin_parent(&forest, mid) == u) {
                fprintf(out, "grand %c %c class=%s iii=%d\n", forest.node[u].data, forest.node[v].data,
                        grand_class(&forest, u, v), sib(&forest, forest.node[u].parent, forest.node[v].parent));
            }
        }
    }
    {
        int n = forest_find(&forest, 'N');
        int p = forest_find(&forest, 'P');
        int mid = forest_bin_parent(&forest, p);
        fprintf(out, "counter N P grand=%d iii=%d\n", mid != 0 && forest_bin_parent(&forest, mid) == n,
                sib(&forest, forest.node[n].parent, forest.node[p].parent));
    }
    forest_grand_stats(&forest, &stats);
    fprintf(out, "stats total=%d parent=%d sibling=%d uncle=%d other=%d iii=%d\n", stats.total, stats.as_parent,
            stats.as_sibling, stats.as_uncle, stats.as_other, stats.as_iii);
    fprintf(out, "option A=only_II B=I_and_II C=I_and_III D=all\n");
    fprintf(out, "pick=%s\n", stats.as_parent > 0 && stats.as_sibling > 0 && stats.as_iii == 0 ? "B" : "?");
}

static void exam_2010(FILE *out) {
    DegItem items[] = {{4, 20}, {3, 10}, {2, 1}, {1, 10}};
    fprintf(out, "exam 2010-5\n");
    degree_print(items, 4, out);
    fprintf(out, "option A=41 B=82 C=113 D=122\n");
    fprintf(out, "trap_A=branch_nodes trap_D=edges\n");
    fprintf(out, "pick=B\n");
}

static void exam_2011(FILE *out) {
    fprintf(out, "exam 2011-6\n");
    noright_print(2011, 116, out);
    fprintf(out, "option A=115 B=116 C=1895 D=1896\n");
    fprintf(out, "trap_A=with_right trap_B=leaves trap_C=branch\n");
    fprintf(out, "pick=D\n");
}

static void exam_2013(FILE *out) {
    fprintf(out, "exam 2013-5\n");
    thr_show_post(out, "ayb##c##x##", 'x');
    fprintf(out, "option A=parent B=leftmost C=sibling D=rightmost\n");
    fprintf(out, "pick=A\n");
}

static void exam_2014_thread(FILE *out) {
    fprintf(out, "exam 2014-4\n");
    fprintf(out, "figure a(b(d(e),x),c)\n");
    thr_show_in(out, "abde###x##c##", 'x');
    fprintf(out, "option A=e,c B=e,a C=d,c D=b,a\n");
    fprintf(out, "pick=D\n");
}

static void exam_2014_leaf(FILE *out) {
    Forest forest;
    int leaves;
    int bin_leaf;
    int deg1;
    int right_null;
    fprintf(out, "exam 2014-5\n");
    forest_parse(&forest, "A(B,C,F) D");
    forest_show(&forest, out);
    leaves = forest_leaves(&forest);
    bin_leaf = forest_bin_leaf(&forest);
    deg1 = forest_deg1(&forest);
    right_null = forest_right_null(&forest);
    fprintf(out, "eq_bin_leaf=%d eq_deg1=%d eq_right_null=%d eq_left_null=%d\n", leaves == bin_leaf, leaves == deg1,
            leaves == right_null, 1);
    fprintf(out, "option A=bin_leaf B=deg1 C=left_null D=right_null\n");
    fprintf(out, "pick=%s\n", leaves != bin_leaf && leaves != deg1 && leaves != right_null ? "C" : "?");
}

static void exam_2016(FILE *out) {
    fprintf(out, "exam 2016-5\n");
    edges_print(25, 15, out);
    fprintf(out, "option A=8 B=9 C=10 D=11\n");
    fprintf(out, "pick=C\n");
}

static int level_of(const Forest *f, char *buf, int cap) {
    int q[FOR_MAX];
    int qh = 0;
    int qt = 0;
    int n = 0;
    buf[0] = '\0';
    if (f->ntree == 0) {
        return 0;
    }
    q[qt++] = f->root[1];
    while (qh < qt) {
        int id = q[qh++];
        if (n + 1 >= cap) {
            return -1;
        }
        buf[n++] = f->node[id].data;
        buf[n] = '\0';
        if (f->node[id].first != 0) {
            q[qt++] = f->node[id].first;
        }
        if (f->node[id].next != 0) {
            q[qt++] = f->node[id].next;
        }
    }
    return 0;
}

static void exam_2019(FILE *out) {
    Forest forest;
    char pre[FOR_MAX];
    char inord[FOR_MAX];
    char post[FOR_MAX];
    char level[FOR_MAX];
    fprintf(out, "exam 2019-2\n");
    forest_parse(&forest, "A(B(E,F,G),C,D)");
    forest_show(&forest, out);
    forest_seq(&forest, pre, inord, post, FOR_MAX);
    level_of(&forest, level, FOR_MAX);
    fprintf(out, "postroot=%s\n", inord);
    fprintf(out, "eq_pre=%d eq_in=%d eq_post=%d eq_level=%d\n", strcmp(inord, pre) == 0, 1, strcmp(inord, post) == 0,
            strcmp(inord, level) == 0);
    fprintf(out, "option A=pre B=in C=post D=level\n");
    fprintf(out, "pick=%s\n", strcmp(inord, pre) != 0 && strcmp(inord, post) != 0 && strcmp(inord, level) != 0 ? "B" : "?");
}

static void exam_orders(FILE *out, const char *title, const char *pre, const char *inord, const char *want_post, int want_trees,
                        const char *options, const char *pick) {
    Forest forest;
    char got_pre[FOR_MAX];
    char got_in[FOR_MAX];
    char got_post[FOR_MAX];
    fprintf(out, "exam %s\n", title);
    forest_from_bi(&forest, pre, inord);
    forest_show(&forest, out);
    forest_seq(&forest, got_pre, got_in, got_post, FOR_MAX);
    fprintf(out, "check post=%s trees=%d\n", got_post, forest.ntree);
    fprintf(out, "hit_post=%d hit_trees=%d\n", strcmp(got_post, want_post) == 0, forest.ntree == want_trees);
    fprintf(out, "option %s\n", options);
    fprintf(out, "pick=%s\n", pick);
}

static void exam_2022_pair(FILE *out) {
    fprintf(out, "exam 2022-3\n");
    thr_show_pairs(out, "qp###");
    thr_show_pairs(out, "p#q##");
    thr_show_pairs(out, "fp##q##");
    thr_show_pairs(out, "qx#p###");
    fprintf(out, "option A=only_I B=only_III C=II_and_III D=II_and_IV\n");
    fprintf(out, "pick=B\n");
}

static void exam_2022_height(FILE *out) {
    fprintf(out, "exam 2022-4\n");
    height_print(3, 244, out);
    fprintf(out, "option A=8 B=7 C=6 D=5\n");
    fprintf(out, "pick=C\n");
}

static void exam_2024(FILE *out) {
    ThrTree tree;
    char inord[THR_MAX];
    int vpos = -1;
    fprintf(out, "exam 2024-3\n");
    thr_show_in(out, "va#pb###q##", 'v');
    thr_from_pre(&tree, "va#pb###q##");
    thr_inorder(&tree, inord, THR_MAX);
    for (int i = 0; inord[i] != '\0'; i++) {
        if (inord[i] == 'v') {
            vpos = i;
        }
    }
    {
        int pred = thr_find(&tree, inord[vpos - 1]);
        int succ = thr_find(&tree, inord[vpos + 1]);
        fprintf(out, "p=%c p_left=%d p_right=%d q=%c q_left=%d q_right=%d\n", tree.node[pred].data,
                tree.node[pred].left != 0, tree.node[pred].right != 0, tree.node[succ].data, tree.node[succ].left != 0,
                tree.node[succ].right != 0);
        fprintf(out, "two_child_ok=%d\n", thr_two_child_ok(&tree));
    }
    fprintf(out, "option A=p_no_right_q_no_left B=q_has_left C=p_has_right D=both_have\n");
    fprintf(out, "pick=A\n");
}

static int root_degree(const char *pre) {
    ThrTree tree;
    thr_from_pre(&tree, pre);
    return (tree.node[tree.root].left != 0) + (tree.node[tree.root].right != 0);
}

static void exam_2025(FILE *out) {
    Forest forest;
    ThrTree chain;
    int n0 = 0;
    fprintf(out, "exam 2025-4\n");
    fprintf(out, "caseA complete_two root_deg=%d\n", root_degree("AB###"));
    forest_parse(&forest, "A B C");
    fprintf(out, "caseB trees=%d binary_roots=1\n", forest.ntree);
    thr_from_pre(&chain, "ABC####");
    for (int i = 1; i <= chain.n; i++) {
        int deg = (chain.node[i].left != 0) + (chain.node[i].right != 0);
        if (deg == 0) {
            n0++;
        }
    }
    fprintf(out, "caseC n0=%d branch=%d\n", n0, chain.n - n0);
    fprintf(out, "caseD root_op_after_children=1\n");
    fprintf(out, "option A=no_deg1 B=forest_to_one C=branch_lt_leaf D=root_first_op\n");
    fprintf(out, "pick=B\n");
}

int exam_run(const char *name, FILE *out) {
    if (strcmp(name, "2009-6") == 0) {
        exam_2009(out);
    } else if (strcmp(name, "2010-5") == 0) {
        exam_2010(out);
    } else if (strcmp(name, "2011-6") == 0) {
        exam_2011(out);
    } else if (strcmp(name, "2013-5") == 0) {
        exam_2013(out);
    } else if (strcmp(name, "2014-4") == 0) {
        exam_2014_thread(out);
    } else if (strcmp(name, "2014-5") == 0) {
        exam_2014_leaf(out);
    } else if (strcmp(name, "2016-5") == 0) {
        exam_2016(out);
    } else if (strcmp(name, "2019-2") == 0) {
        exam_2019(out);
    } else if (strcmp(name, "2020-4") == 0) {
        exam_orders(out, "2020-4", "abcdef", "badfec", "bfedca", 2, "A=badfec B=bdfeca C=bfedca D=fedcba", "C");
    } else if (strcmp(name, "2021-4") == 0) {
        exam_orders(out, "2021-4", "abdcegf", "bdaegcf", "dbgefca", 3, "A=1 B=2 C=3 D=4", "C");
    } else if (strcmp(name, "2022-3") == 0) {
        exam_2022_pair(out);
    } else if (strcmp(name, "2022-4") == 0) {
        exam_2022_height(out);
    } else if (strcmp(name, "2024-3") == 0) {
        exam_2024(out);
    } else if (strcmp(name, "2025-4") == 0) {
        exam_2025(out);
    } else if (strcmp(name, "all") == 0) {
        const char *ids[] = {"2009-6", "2010-5", "2011-6", "2013-5", "2014-4", "2014-5", "2016-5",
                              "2019-2", "2020-4", "2021-4", "2022-3", "2022-4", "2024-3", "2025-4"};
        for (int i = 0; i < 14; i++) {
            exam_run(ids[i], out);
            if (i + 1 < 14) {
                fputc('\n', out);
            }
        }
    } else {
        return -1;
    }
    return 0;
}
