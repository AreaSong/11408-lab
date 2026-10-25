#include "check.h"

#include <string.h>

/* 返回这个结点交给父亲的黑结点数。NIL 是黑叶，贡献 1。子树已经坏了就返回 -1。 */
static int side_black(const RbTree *t, const RbNode *n, int *ok) {
    int left;
    int right;
    if (n == t->nil) {
        return 1;
    }
    left = side_black(t, n->left, ok);
    right = side_black(t, n->right, ok);
    if (left < 0 || right < 0 || left != right) {
        *ok = 0;
        return -1;
    }
    return left + (n->red ? 0 : 1);
}

static int count_nodes(const RbTree *t, const RbNode *n) {
    if (n == t->nil) {
        return 0;
    }
    return 1 + count_nodes(t, n->left) + count_nodes(t, n->right);
}

static int count_reds(const RbTree *t, const RbNode *n) {
    if (n == t->nil) {
        return 0;
    }
    return (n->red ? 1 : 0) + count_reds(t, n->left) + count_reds(t, n->right);
}

static int height_of(const RbTree *t, const RbNode *n) {
    int left;
    int right;
    if (n == t->nil) {
        return 0;
    }
    left = height_of(t, n->left);
    right = height_of(t, n->right);
    return (left > right ? left : right) + 1;
}

typedef struct PathAcc {
    int long_path;
    int short_path;
    int seen;
} PathAcc;

static void walk_paths(const RbTree *t, const RbNode *n, int edges, PathAcc *acc) {
    if (n == t->nil) {
        if (!acc->seen || edges > acc->long_path) {
            acc->long_path = edges;
        }
        if (!acc->seen || edges < acc->short_path) {
            acc->short_path = edges;
        }
        acc->seen = 1;
        return;
    }
    walk_paths(t, n->left, edges + 1, acc);
    walk_paths(t, n->right, edges + 1, acc);
}

static int bst_ok(const RbTree *t, const RbNode *n, int *has, int *prev) {
    if (n == t->nil) {
        return 1;
    }
    if (!bst_ok(t, n->left, has, prev)) {
        return 0;
    }
    if (*has && n->key <= *prev) {
        return 0;
    }
    *has = 1;
    *prev = n->key;
    return bst_ok(t, n->right, has, prev);
}

static int colors_ok(const RbTree *t, const RbNode *n) {
    if (n == t->nil) {
        return 1;
    }
    if (n->red != 0 && n->red != 1) {
        return 0;
    }
    return colors_ok(t, n->left) && colors_ok(t, n->right);
}

static int no_red_red(const RbTree *t, const RbNode *n, RbStats *s) {
    if (n == t->nil) {
        return 1;
    }
    if (n->red && n->left->red) {
        if (!s->has_pair) {
            s->pair_parent = n->key;
            s->pair_child = n->left->key;
            s->has_pair = 1;
        }
        return 0;
    }
    if (n->red && n->right->red) {
        if (!s->has_pair) {
            s->pair_parent = n->key;
            s->pair_child = n->right->key;
            s->has_pair = 1;
        }
        return 0;
    }
    if (!no_red_red(t, n->left, s)) {
        return 0;
    }
    return no_red_red(t, n->right, s);
}

static void fill_bh(const RbTree *t, RbStats *s) {
    int left = 1;
    int right = 1;
    s->bh_ok = 1;
    s->bh = 0;
    if (t->root == t->nil) {
        return;
    }
    left = side_black(t, t->root->left, &s->bh_ok);
    right = side_black(t, t->root->right, &s->bh_ok);
    if (!s->bh_ok || left != right) {
        s->bh_ok = 0;
        s->bh = -1;
        return;
    }
    s->bh = left;
}

void rb_stats(const RbTree *t, RbStats *s) {
    PathAcc acc = {0, 0, 0};
    int has = 0;
    int prev = 0;
    memset(s, 0, sizeof *s);
    s->nodes = count_nodes(t, t->root);
    s->reds = count_reds(t, t->root);
    s->height = height_of(t, t->root);
    walk_paths(t, t->root, 0, &acc);
    s->long_path = acc.long_path;
    s->short_path = acc.short_path;
    s->bst = bst_ok(t, t->root, &has, &prev);
    s->color_ok = colors_ok(t, t->root);
    s->nil_black = !t->nil->red;
    if (t->root == t->nil) {
        s->root_state = 0;
    } else if (t->root->red) {
        s->root_state = 2;
    } else {
        s->root_state = 1;
    }
    s->root_black = s->root_state != 2;
    s->redred = no_red_red(t, t->root, s);
    fill_bh(t, s);
    s->rb = s->bst && s->color_ok && s->root_black && s->nil_black && s->redred && s->bh_ok;
}

static void print_side(FILE *out, char name, int value) {
    if (value < 0) {
        fprintf(out, "%c=bad", name);
    } else {
        fprintf(out, "%c=%d", name, value);
    }
}

/* 后序：先孩子后自己，和「自底向上对黑高」的顺序一致。返回值是交给父亲的黑结点数。 */
static int dump_walk(const RbTree *t, const RbNode *n, FILE *out) {
    int left;
    int right;
    int bad;
    if (n == t->nil) {
        return 1;
    }
    left = dump_walk(t, n->left, out);
    right = dump_walk(t, n->right, out);
    bad = left < 0 || right < 0 || left != right;
    fprintf(out, "node %d %c ", n->key, n->red ? 'R' : 'B');
    print_side(out, 'L', left);
    fputc(' ', out);
    print_side(out, 'R', right);
    if (bad) {
        fprintf(out, " bh=bad\n");
        return -1;
    }
    fprintf(out, " bh=%d\n", left);
    return left + (n->red ? 0 : 1);
}

void rb_dump(const RbTree *t, FILE *out) {
    dump_walk(t, t->root, out);
}
