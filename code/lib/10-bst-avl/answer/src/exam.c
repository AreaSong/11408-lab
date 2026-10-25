#include "exam.h"

#include "avl.h"
#include "bst.h"
#include "node.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

static void finish(FILE *out, const char *option, char pick) {
    fprintf(out, "option %s\n", option);
    fprintf(out, "pick=%c\n", pick);
}

static void print_dec(FILE *out, const char *name, long long num, long long den) {
    long long scaled = (num * 100 + den / 2) / den;
    fprintf(out, "%s=%lld/%lld dec=%lld.%02lld\n", name, num, den, scaled / 100, scaled % 100);
}

static int avl_ok(const Node *t) {
    return node_bst_ok(t) && node_height_ok(t);
}

static Node *put_all(const int *keys, int n, int avl, int desc, FILE *out) {
    Node *t = NULL;
    for (int i = 0; i < n; i++) {
        int dup = 0;
        if (out) {
            fprintf(out, "insert %d\n", keys[i]);
        }
        t = avl ? avl_insert(t, keys[i], desc, out, &dup) : bst_insert(t, keys[i], &dup);
        if (out) {
            fprintf(out, "tree ");
            node_fprint(t, out, avl);
            fputc('\n', out);
        }
    }
    return t;
}

static int exam_2010(FILE *out) {
    static const int keys[] = {24, 13, 53, 37, 90, 48};
    const Node *p;
    char pick = '?';
    Node *t;
    fprintf(out, "exam 2010-4\n");
    t = put_all(keys, 6, 1, 0, out);
    p = node_find(t, 37);
    if (p && p->left && p->right) {
        fprintf(out, "left=%d right=%d\n", p->left->key, p->right->key);
        if (p->left->key == 24 && p->right->key == 53) {
            pick = 'C';
        }
    }
    finish(out, "A=13,48 B=24,48 C=24,53 D=24,90", pick);
    node_free(t);
    return 0;
}

static int bf0_branch(const Node *t) {
    if (!t || (!t->left && !t->right)) {
        return 0;
    }
    return (node_bf(t) == 0) + bf0_branch(t->left) + bf0_branch(t->right);
}

static void list_bf0(const Node *t, FILE *out, int *first) {
    if (!t || (!t->left && !t->right)) {
        return;
    }
    list_bf0(t->left, out, first);
    if (node_bf(t) == 0) {
        if (!*first) {
            fputc(' ', out);
        }
        *first = 0;
        fprintf(out, "%d", t->key);
    }
    list_bf0(t->right, out, first);
}

static int exam_2012(FILE *out) {
    Node *t = avl_build_min(6);
    int ok = avl_min_count(6) == 20 && node_count(t) == 20 && node_height(t) == 6 && avl_ok(t);
    fprintf(out, "exam 2012-4\n");
    avl_min_report(out, 6);
    finish(out, "A=12 B=20 C=32 D=33", ok ? 'B' : '?');
    node_free(t);
    return 0;
}

static int exam_2013_avl(FILE *out) {
    static const int keys[] = {1, 2, 3, 4, 5, 6, 7};
    int first = 1;
    int nbf;
    Node *t;
    fprintf(out, "exam 2013-3\n");
    t = put_all(keys, 7, 1, 0, out);
    nbf = bf0_branch(t);
    fprintf(out, "bf0=%d keys=", nbf);
    list_bf0(t, out, &first);
    fputc('\n', out);
    finish(out, "A=0 B=1 C=2 D=3", nbf == 3 ? 'D' : '?');
    node_free(t);
    return 0;
}

static int bst_round(const int *keys, int n, int victim, FILE *out) {
    Node *orig;
    Node *t = NULL;
    int found = 0;
    int dup = 0;
    int same;
    for (int i = 0; i < n; i++) {
        t = bst_insert(t, keys[i], &dup);
    }
    orig = node_clone(t);
    fprintf(out, "before ");
    node_fprint(t, out, 0);
    fputc('\n', out);
    t = bst_delete(t, victim, out, &found);
    fprintf(out, "mid ");
    node_fprint(t, out, 0);
    fputc('\n', out);
    t = bst_insert(t, victim, &dup);
    same = found && node_equal(orig, t);
    fprintf(out, "after ");
    node_fprint(t, out, 0);
    fprintf(out, "\nsame=%s\n", same ? "yes" : "no");
    node_free(t);
    node_free(orig);
    return same;
}

static int exam_2013_bst(FILE *out) {
    static const int keys[] = {5, 3, 7, 2, 4, 6};
    int leaf;
    int one;
    int two;
    fprintf(out, "exam 2013-6\nleaf 2\n");
    leaf = bst_round(keys, 6, 2, out);
    fprintf(out, "one 7\n");
    one = bst_round(keys, 6, 7, out);
    fprintf(out, "two 3\n");
    two = bst_round(keys, 6, 3, out);
    finish(out, "A=I,III B=I,IV C=II,III D=II,IV", (leaf && !one && !two) ? 'C' : '?');
    return 0;
}

static void named(const Node *t, FILE *out) {
    static const char *nm[] = {"?", "do", "for", "repeat", "while"};
    if (!t) {
        fputc('#', out);
        return;
    }
    fputs(nm[t->key], out);
    if (!t->left && !t->right) {
        return;
    }
    fputc('(', out);
    named(t->left, out);
    fputc(',', out);
    named(t->right, out);
    fputc(')', out);
}

static long long weighted(const int *keys, const int *w, int n) {
    Node *t = NULL;
    long long num = 0;
    long long den = 0;
    int dup = 0;
    for (int i = 0; i < n; i++) {
        t = bst_insert(t, keys[i], &dup);
    }
    bst_asl_weight(t, keys, w, n, &num, &den);
    node_free(t);
    return num;
}

static int exam_2013_asl(FILE *out) {
    static const int half_lv[] = {2, 1, 2, 3};
    static const int half_w[] = {35, 15, 15, 35};
    static const int seq_w[] = {35, 35, 15, 15};
    static const int for_keys[] = {2, 1, 4, 3};
    static const int for_w[] = {15, 35, 35, 15};
    static const int rep_keys[] = {3, 1, 4, 2};
    static const int rep_w[] = {15, 35, 35, 15};
    long long half = 0;
    long long seq = 0;
    Node *for_root = NULL;
    Node *rep_root = NULL;
    int dup = 0;
    fprintf(out, "exam 2013-42\nhalf_level do=2 for=1 repeat=2 while=3\n");
    for (int i = 0; i < 4; i++) {
        half += (long long)half_lv[i] * half_w[i];
        seq += (long long)(i + 1) * seq_w[i];
    }
    print_dec(out, "half", half, 100);
    fprintf(out, "seq do while for repeat\n");
    print_dec(out, "seq", seq, 100);
    for (int i = 0; i < 4; i++) {
        for_root = bst_insert(for_root, for_keys[i], &dup);
        rep_root = bst_insert(rep_root, rep_keys[i], &dup);
    }
    fprintf(out, "for_root ");
    named(for_root, out);
    fputc('\n', out);
    print_dec(out, "for_root", weighted(for_keys, for_w, 4), 100);
    fprintf(out, "repeat_root ");
    named(rep_root, out);
    fputc('\n', out);
    print_dec(out, "repeat_root", weighted(rep_keys, rep_w, 4), 100);
    node_free(for_root);
    node_free(rep_root);
    return 0;
}

static const Node *seek_any(const Node *t, int key) {
    const Node *p;
    if (!t) {
        return NULL;
    }
    if (t->key == key) {
        return t;
    }
    p = seek_any(t->left, key);
    return p ? p : seek_any(t->right, key);
}

static int extreme(const Node *t, int want_max) {
    int m = t->key;
    if (t->left) {
        int v = extreme(t->left, want_max);
        if (want_max ? v > m : v < m) {
            m = v;
        }
    }
    if (t->right) {
        int v = extreme(t->right, want_max);
        if (want_max ? v > m : v < m) {
            m = v;
        }
    }
    return m;
}

static int descending(const Node *t, int *prev, int *has) {
    if (!t) {
        return 1;
    }
    if (!descending(t->left, prev, has) || (*has && t->key >= *prev)) {
        return 0;
    }
    *has = 1;
    *prev = t->key;
    return descending(t->right, prev, has);
}

static void dump_desc(FILE *out, const char *title, const int *keys, int n) {
    Node *t;
    int prev = 0;
    int has = 0;
    fprintf(out, "%s\n", title);
    t = put_all(keys, n, 1, 1, out);
    fprintf(out, "inorder ");
    node_inorder(t, out);
    fprintf(out, "\norder=%s height_ok=%s\n", descending(t, &prev, &has) ? "desc" : "bad",
            node_height_ok(t) ? "yes" : "no");
    node_free(t);
}

static int exam_2015(FILE *out) {
    static const int pair[] = {1, 2};
    static const int triple[] = {3, 1, 2};
    Node *two;
    Node *three;
    const Node *min_node;
    const Node *max_node;
    const Node *last;
    int prev = 0;
    int has = 0;
    int ok;
    fprintf(out, "exam 2015-4\n");
    dump_desc(out, "pair", pair, 2);
    two = put_all(pair, 2, 1, 1, NULL);
    min_node = seek_any(two, extreme(two, 0));
    max_node = seek_any(two, extreme(two, 1));
    fprintf(out, "root_degree=%d\n", (two->left != NULL) + (two->right != NULL));
    fprintf(out, "min=%d leaf=%s\n", min_node->key, (!min_node->left && !min_node->right) ? "yes" : "no");
    fprintf(out, "max=%d left=%s\n", max_node->key, max_node->left ? "yes" : "no");
    dump_desc(out, "triple", triple, 3);
    three = put_all(triple, 3, 1, 1, NULL);
    last = seek_any(three, 2);
    max_node = seek_any(three, extreme(three, 1));
    fprintf(out, "last=2 leaf=%s\n", (!last->left && !last->right) ? "yes" : "no");
    fprintf(out, "max=%d left=%s\n", max_node->key, max_node->left ? "yes" : "no");
    ok = (two->left != NULL) + (two->right != NULL) == 1;
    ok = ok && (min_node->left || min_node->right);
    ok = ok && !max_node->left && last && (last->left || last->right);
    ok = ok && descending(three, &prev, &has) && node_height_ok(two) && node_height_ok(three);
    finish(out, "A=root_degree_2 B=min_is_leaf C=last_is_leaf D=max_no_left", ok ? 'D' : '?');
    node_free(two);
    node_free(three);
    return 0;
}

static int exam_2020(FILE *out) {
    static const int seq[4][5] = {
        {4, 5, 2, 1, 3},
        {4, 5, 1, 2, 3},
        {4, 2, 5, 3, 1},
        {4, 2, 1, 3, 5},
    };
    char shape[4][64];
    int ok;
    fprintf(out, "exam 2020-5\n");
    for (int i = 0; i < 4; i++) {
        Node *t = put_all(seq[i], 5, 0, 0, NULL);
        node_shape(t, shape[i], (int)sizeof shape[i], 0);
        fprintf(out, "%c %s\n", 'A' + i, shape[i]);
        node_free(t);
    }
    ok = strcmp(shape[0], shape[2]) == 0 && strcmp(shape[0], shape[3]) == 0 &&
         strcmp(shape[0], shape[1]) != 0;
    finish(out, "A=4,5,2,1,3 B=4,5,1,2,3 C=4,2,5,3,1 D=4,2,1,3,5", ok ? 'B' : '?');
    return 0;
}

static int exam_2024(FILE *out) {
    static const int keys[] = {10, 30, 20, 25};
    Node *t = put_all(keys, 4, 0, 0, NULL);
    int x = 25;
    int a = x < 10;
    int b = x > 30;
    int c = x > 10 && x < 20;
    int d = x > 20 && x < 30;
    fprintf(out, "exam 2024-7\ntree ");
    node_fprint(t, out, 0);
    fprintf(out, "\nk1=10 k2=30 k3=20 sample=%d\n", x);
    fprintf(out, "A=%s\nB=%s\nC=%s\nD=%s\n", a ? "yes" : "no", b ? "yes" : "no", c ? "yes" : "no",
            d ? "yes" : "no");
    finish(out, "A=X<K1 B=X>K2 C=K1<X<K3 D=K3<X<K2", (!a && !b && !c && d) ? 'D' : '?');
    node_free(t);
    return 0;
}

int exam_run(const char *name, FILE *out) {
    static const struct {
        const char *name;
        int (*fn)(FILE *out);
    } table[] = {
        {"2010-4", exam_2010}, {"2012-4", exam_2012},   {"2013-3", exam_2013_avl},
        {"2013-6", exam_2013_bst}, {"2013-42", exam_2013_asl}, {"2015-4", exam_2015},
        {"2019-4", exam_2019}, {"2020-5", exam_2020}, {"2024-7", exam_2024},
    };
    int n = (int)(sizeof table / sizeof table[0]);
    if (strcmp(name, "all") == 0) {
        for (int i = 0; i < n; i++) {
            if (i) {
                fputc('\n', out);
            }
            table[i].fn(out);
        }
        return 0;
    }
    for (int i = 0; i < n; i++) {
        if (strcmp(name, table[i].name) == 0) {
            return table[i].fn(out);
        }
    }
    return -1;
}
