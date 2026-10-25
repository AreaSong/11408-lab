#include "forest.h"

#include <stdio.h>
#include <string.h>

int forest_walk_bin(const Forest *f, int id, int order, char *buf, int cap, int n);
int forest_walk_tree(const Forest *f, int id, int post, char *buf, int cap, int n);

static int push_ch(char *buf, int cap, int n, char ch) {
    if (n < 0 || n + 1 >= cap) {
        return -1;
    }
    buf[n] = ch;
    buf[n + 1] = '\0';
    return n + 1;
}

static void print_parent(const Forest *f, FILE *out) {
    for (int i = 1; i <= f->n; i++) {
        fprintf(out, "parent i=%d data=%c parent=", i, f->node[i].data);
        if (f->node[i].parent < 0) {
            fprintf(out, "-\n");
        } else {
            fprintf(out, "%c\n", f->node[f->node[i].parent].data);
        }
    }
}

static void print_kids(const Forest *f, int id, FILE *out) {
    int child = f->node[id].first;
    if (child == 0) {
        fprintf(out, "-");
        return;
    }
    for (; child != 0; child = f->node[child].next) {
        if (child != f->node[id].first) {
            fputc(' ', out);
        }
        fputc(f->node[child].data, out);
    }
}

static void print_store(const Forest *f, FILE *out) {
    print_parent(f, out);
    for (int i = 1; i <= f->n; i++) {
        fprintf(out, "child %c: ", f->node[i].data);
        print_kids(f, i, out);
        fputc('\n', out);
    }
    for (int i = 1; i <= f->n; i++) {
        fprintf(out, "cs %c child=", f->node[i].data);
        if (f->node[i].first == 0) {
            fprintf(out, "-");
        } else {
            fputc(f->node[f->node[i].first].data, out);
        }
        fprintf(out, " sibling=");
        if (f->node[i].next == 0) {
            fprintf(out, "-\n");
        } else {
            fprintf(out, "%c\n", f->node[f->node[i].next].data);
        }
    }
}

static int write_tree(const Forest *f, int id, char *buf, int cap, int n) {
    int child;
    n = push_ch(buf, cap, n, f->node[id].data);
    if (f->node[id].first == 0) {
        return n;
    }
    n = push_ch(buf, cap, n, '(');
    for (child = f->node[id].first; child != 0; child = f->node[child].next) {
        if (child != f->node[id].first) {
            n = push_ch(buf, cap, n, ',');
        }
        n = write_tree(f, child, buf, cap, n);
    }
    return push_ch(buf, cap, n, ')');
}

static void print_shape(const Forest *f, FILE *out) {
    char buf[FOR_MAX * 4];
    int n = 0;
    buf[0] = '\0';
    for (int i = 1; i <= f->ntree; i++) {
        if (i > 1) {
            n = push_ch(buf, (int)sizeof buf, n, ' ');
        }
        n = write_tree(f, f->root[i], buf, (int)sizeof buf, n);
    }
    fprintf(out, "shape %s\n", buf);
}

static void print_bin_at(const Forest *f, int id, FILE *out) {
    if (id == 0) {
        return;
    }
    fprintf(out, "bin %c left=%c right=%c\n", f->node[id].data,
            f->node[id].first == 0 ? '-' : f->node[f->node[id].first].data,
            f->node[id].next == 0 ? '-' : f->node[f->node[id].next].data);
    print_bin_at(f, f->node[id].first, out);
    print_bin_at(f, f->node[id].next, out);
}

static void print_trees(const Forest *f, FILE *out) {
    for (int i = 1; i <= f->ntree; i++) {
        char one_pre[FOR_MAX];
        char one_post[FOR_MAX];
        one_pre[0] = '\0';
        one_post[0] = '\0';
        forest_walk_tree(f, f->root[i], 0, one_pre, FOR_MAX, 0);
        forest_walk_tree(f, f->root[i], 1, one_post, FOR_MAX, 0);
        fprintf(out, "tree %d root=%c pre=%s post=%s\n", i, f->node[f->root[i]].data, one_pre, one_post);
    }
}

static void print_orders(const Forest *f, FILE *out) {
    char pre[FOR_MAX];
    char inord[FOR_MAX];
    char post[FOR_MAX];
    char bin_pre[FOR_MAX];
    char bin_in[FOR_MAX];
    char level[FOR_MAX];
    int q[FOR_MAX];
    int qh = 0;
    int qt = 0;
    int ln = 0;
    forest_seq(f, pre, inord, post, FOR_MAX);
    bin_pre[0] = '\0';
    bin_in[0] = '\0';
    if (f->ntree > 0) {
        forest_walk_bin(f, f->root[1], 0, bin_pre, FOR_MAX, 0);
        forest_walk_bin(f, f->root[1], 1, bin_in, FOR_MAX, 0);
        q[qt++] = f->root[1];
    }
    level[0] = '\0';
    while (qh < qt) {
        int id = q[qh++];
        ln = push_ch(level, FOR_MAX, ln, f->node[id].data);
        if (f->node[id].first != 0) {
            q[qt++] = f->node[id].first;
        }
        if (f->node[id].next != 0) {
            q[qt++] = f->node[id].next;
        }
    }
    fprintf(out, "forest_pre=%s\n", pre[0] ? pre : "-");
    fprintf(out, "forest_in=%s\n", inord[0] ? inord : "-");
    fprintf(out, "bin_pre=%s\n", bin_pre[0] ? bin_pre : "-");
    fprintf(out, "bin_in=%s\n", bin_in[0] ? bin_in : "-");
    fprintf(out, "bin_post=%s\n", post[0] ? post : "-");
    fprintf(out, "bin_level=%s\n", level[0] ? level : "-");
    fprintf(out, "match_pre=%d match_in=%d\n", strcmp(pre, bin_pre) == 0, strcmp(inord, bin_in) == 0);
}

void forest_show(const Forest *f, FILE *out) {
    int leaves = forest_leaves(f);
    fprintf(out, "forest n=%d trees=%d kind=%s\n", f->n, f->ntree, f->ntree <= 1 ? "tree" : "forest");
    print_store(f, out);
    print_shape(f, out);
    if (f->ntree > 0) {
        print_bin_at(f, f->root[1], out);
    }
    print_trees(f, out);
    print_orders(f, out);
    fprintf(out, "leaves=%d left_null=%d bin_leaf=%d deg1=%d right_null=%d\n", leaves, leaves, forest_bin_leaf(f),
            forest_deg1(f), forest_right_null(f));
    fprintf(out, "right_formula=%d\n", f->n - leaves + 1);
    if (f->ntree > 0) {
        int sib = f->node[f->root[1]].next;
        fprintf(out, "root_sibling=%c\n", sib == 0 ? '-' : f->node[sib].data);
    }
}
