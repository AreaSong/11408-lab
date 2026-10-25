#include "exam.h"

#include "build.h"
#include "expr.h"
#include "prop.h"
#include "tree.h"
#include "wpl.h"

#include <string.h>

static void exam_2009(FILE *out) {
    fprintf(out, "exam 2009-5\n");
    most_print(6, 8, out);
    fprintf(out, "option A=39 B=52 C=111 D=119\n");
    fprintf(out, "trap_A=min_nodes\n");
    fprintf(out, "trap_D=127-8\n");
    fprintf(out, "pick=C\n");
}

static void exam_2011_4(FILE *out) {
    fprintf(out, "exam 2011-4\n");
    cbt_print(768, 0, 0, out);
    fprintf(out, "option A=257 B=258 C=384 D=385\n");
    fprintf(out, "trap_A=bottom_only\n");
    fprintf(out, "pick=C\n");
}

static int hit_in(const BtForm *forms, int n, const char *inord) {
    for (int i = 0; i < n; i++) {
        if (strcmp(forms[i].inord, inord) == 0) {
            return 1;
        }
    }
    return 0;
}

static void exam_2011_5(FILE *out) {
    BtForm forms[PREPOST_MAX];
    int n = prepost_forms("1234", "4321", forms, PREPOST_MAX);
    const char *opt[] = {"1234", "2341", "3241", "4321"};
    const char *name[] = {"A", "B", "C", "D"};
    fprintf(out, "exam 2011-5\n");
    fprintf(out, "pre=1234 post=4321 count=%d\n", n);
    for (int i = 0; i < n; i++) {
        fprintf(out, "tree %d shape=%s in=%s\n", i + 1, forms[i].shape, forms[i].inord);
    }
    for (int k = 0; k < 4; k++) {
        fprintf(out, "option %s in=%s hit=%d\n", name[k], opt[k], hit_in(forms, n, opt[k]));
    }
    fprintf(out, "pick=C\n");
}

static void print_case(const char *name, const char *link, FILE *out) {
    BiTree tree;
    TreeInfo info;
    char pre[BT_MAX];
    char inord[BT_MAX];
    char post[BT_MAX];
    tree_from_pre(&tree, link);
    tree_info(&tree, &info, pre, inord, post, BT_MAX);
    (void)post;
    fprintf(out, "case %s link=%s pre=%s in=%s equal=%d no_left=%d no_right=%d nonleaf=%d\n", name, link, pre,
            inord, info.pre_eq_in, info.no_left, info.no_right, info.nonleaf);
    fprintf(out, "flags nonleaf_deg1=%d nonleaf_deg2=%d all_deg1=%d all_deg2=%d\n", info.nonleaf_deg1,
            info.nonleaf_deg2, info.all_deg1, info.all_deg2);
}

static void exam_2017_4(FILE *out) {
    fprintf(out, "exam 2017-4\n");
    print_case("right", "A#B#C##", out);
    print_case("left", "ABC####", out);
    print_case("both", "AB##C##", out);
    print_case("one", "A##", out);
    fprintf(out, "note all_deg1=0 because a leaf has degree 0\n");
    fprintf(out, "pick=B\n");
}

static void one_wpl(FILE *out, const char *title, int argc, char **argv) {
    int sum = 0;
    fprintf(out, "tree %s\n", title);
    wpl_eval(argc, argv, &sum, out);
}

static void exam_2014(FILE *out) {
    char *sample[] = {"4", "2", "5", "#", "#", "1", "#", "#", "3", "#", "#"};
    char *one[] = {"7", "#", "#"};
    char *inner[] = {"100", "2", "5", "#", "#", "1", "#", "#", "3", "#", "#"};
    fprintf(out, "exam 2014-41\n");
    one_wpl(out, "sample", 11, sample);
    one_wpl(out, "single", 3, one);
    one_wpl(out, "ignore_inner", 11, inner);
}

static void one_expr(FILE *out, const char *title, int argc, char **argv) {
    char buf[128];
    fprintf(out, "tree %s\n", title);
    expr_eval(argc, argv, buf, (int)sizeof buf, out);
}

static void exam_2017_41(FILE *out) {
    char *first[] = {"*", "+", "a", "#", "#", "b", "#", "#", "*", "c", "#", "#", "-", "#", "d", "#", "#"};
    char *second[] = {"+", "*", "a", "#", "#", "b", "#", "#", "-", "#", "-", "c", "#", "#", "d", "#", "#"};
    fprintf(out, "exam 2017-41\n");
    one_expr(out, "first", 17, first);
    one_expr(out, "second", 17, second);
}

int exam_run(const char *name, FILE *out) {
    if (strcmp(name, "2009-5") == 0) {
        exam_2009(out);
    } else if (strcmp(name, "2011-4") == 0) {
        exam_2011_4(out);
    } else if (strcmp(name, "2011-5") == 0) {
        exam_2011_5(out);
    } else if (strcmp(name, "2017-4") == 0) {
        exam_2017_4(out);
    } else if (strcmp(name, "2014-41") == 0) {
        exam_2014(out);
    } else if (strcmp(name, "2017-41") == 0) {
        exam_2017_41(out);
    } else {
        return -1;
    }
    return 0;
}
