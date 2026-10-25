#include "avl.h"

#include <stdlib.h>

static void trace_line(FILE *out, const char *tag, const Node *t) {
    if (!out) {
        return;
    }
    fprintf(out, "%s ", tag);
    node_fprint(t, out, 1);
    fputc('\n', out);
}

static void trace_pivot(FILE *out, const Node *t, const Node *child, const char *kind) {
    if (!out) {
        return;
    }
    /* 这里的 t 是最小不平衡子树的根，不是整棵树的根。 */
    fprintf(out, "pivot %d bf=%d child=%d childbf=%d kind=%s\n", t->key, node_bf(t), child->key,
            node_bf(child), kind);
    trace_line(out, "unbal", t);
}

/* 左旋：右孩子升上来。RR 的单旋，也是 RL 的第二步。 */
static Node *rotate_left(Node *x) {
    Node *y = x->right;
    x->right = y->left;
    y->left = x;
    node_pull(x);
    node_pull(y);
    return y;
}

/* 右旋：左孩子升上来。LL 的单旋，也是 LR 的第二步。 */
static Node *rotate_right(Node *y) {
    Node *x = y->left;
    y->left = x->right;
    x->right = y;
    node_pull(y);
    node_pull(x);
    return x;
}

static Node *fix_left(Node *t, FILE *out) {
    int cb = node_bf(t->left);
    /*
     * 左高。孩子的平衡因子 >= 0 时，增高发生在左孩子的左边（或两边一样高），右单旋即可。
     * 插入时孩子的平衡因子不会是 0；删除时可以是 0，仍然做单旋，不要误判成 LR。
     */
    trace_pivot(out, t, t->left, cb >= 0 ? "LL" : "LR");
    if (cb < 0) {
        t->left = rotate_left(t->left);
        trace_line(out, "half", t);
    }
    t = rotate_right(t);
    trace_line(out, "balanced", t);
    return t;
}

static Node *fix_right(Node *t, FILE *out) {
    int cb = node_bf(t->right);
    trace_pivot(out, t, t->right, cb <= 0 ? "RR" : "RL");
    if (cb > 0) {
        t->right = rotate_right(t->right);
        trace_line(out, "half", t);
    }
    t = rotate_left(t);
    trace_line(out, "balanced", t);
    return t;
}

static Node *rebalance(Node *t, FILE *out) {
    int b;
    node_pull(t);
    b = node_bf(t);
    if (b > 1) {
        return fix_left(t, out);
    }
    if (b < -1) {
        return fix_right(t, out);
    }
    return t;
}

static Node *insert_rec(Node *t, int key, int desc, FILE *out, int *dup) {
    int to_left;
    if (!t) {
        return node_new(key);
    }
    if (key == t->key) {
        *dup = 1;
        return t;
    }
    to_left = desc ? key > t->key : key < t->key;
    if (to_left) {
        t->left = insert_rec(t->left, key, desc, out, dup);
    } else {
        t->right = insert_rec(t->right, key, desc, out, dup);
    }
    if (*dup) {
        return t;
    }
    /* 从插入点往回走。插入把子树高度补回原值，所以最低的失衡点转完，上面就不会再失衡。 */
    return rebalance(t, out);
}

Node *avl_insert(Node *root, int key, int desc, FILE *out, int *dup) {
    *dup = 0;
    return insert_rec(root, key, desc, out, dup);
}

static Node *detach_pred(Node *t, int *key_out, FILE *out) {
    if (!t->right) {
        Node *left = t->left;
        *key_out = t->key;
        free(t);
        return left;
    }
    t->right = detach_pred(t->right, key_out, out);
    return rebalance(t, out);
}

static Node *delete_rec(Node *t, int key, FILE *out, int *found, int report) {
    int pred = 0;
    if (!t) {
        return NULL;
    }
    if (key < t->key) {
        t->left = delete_rec(t->left, key, out, found, report);
    } else if (key > t->key) {
        t->right = delete_rec(t->right, key, out, found, report);
    } else if (!t->left || !t->right) {
        Node *child = t->left ? t->left : t->right;
        *found = 1;
        if (report && out) {
            const char *name = "leaf";
            if (t->left) {
                name = "left";
            } else if (t->right) {
                name = "right";
            }
            fprintf(out, "case %s\n", name);
        }
        free(t);
        return child;
    } else {
        Node *pred_node = t->left;
        while (pred_node->right) {
            pred_node = pred_node->right;
        }
        pred = pred_node->key;
        *found = 1;
        /* 先说明用的是哪一个前驱，再摘掉它。摘的过程里如果失衡，旋转记在这行后面。 */
        if (report && out) {
            fprintf(out, "case two\npred %d\n", pred);
        }
        t->left = detach_pred(t->left, &pred, out);
        t->key = pred;
    }
    if (!*found) {
        return t;
    }
    /* 删除会让子树变矮，矮完之后祖先仍可能失衡，所以一路都要看，不像插入只转一处。 */
    return rebalance(t, out);
}

Node *avl_delete(Node *root, int key, FILE *out, int *found) {
    *found = 0;
    return delete_rec(root, key, out, found, 1);
}

int avl_min_count(int h) {
    int prev = 0;
    int cur = 1;
    if (h <= 0) {
        return 0;
    }
    if (h == 1) {
        return 1;
    }
    for (int i = 2; i <= h; i++) {
        int next = cur + prev + 1;
        prev = cur;
        cur = next;
    }
    return cur;
}

static Node *build_rec(int h, int *next) {
    Node *t;
    if (h <= 0) {
        return NULL;
    }
    t = node_new(0);
    /* 先造更高的左子树，关键字按中序递增，造出来的才是二叉排序树。 */
    t->left = build_rec(h - 1, next);
    t->key = (*next)++;
    t->right = build_rec(h - 2, next);
    node_pull(t);
    return t;
}

Node *avl_build_min(int h) {
    int next = 1;
    return build_rec(h, &next);
}

static void tally(const Node *t, int *nonleaf, int *bf1) {
    if (!t || (!t->left && !t->right)) {
        return;
    }
    (*nonleaf)++;
    if (node_bf(t) == 1) {
        (*bf1)++;
    }
    tally(t->left, nonleaf, bf1);
    tally(t->right, nonleaf, bf1);
}

void avl_min_report(FILE *out, int h) {
    for (int i = 1; i <= h; i++) {
        Node *t = avl_build_min(i);
        int nonleaf = 0;
        int bf1 = 0;
        int avl_ok = node_bst_ok(t) && node_height_ok(t);
        tally(t, &nonleaf, &bf1);
        fprintf(out, "h=%d recur=%d build=%d height=%d avl=%s bf1=%d/%d\n", i, avl_min_count(i),
                node_count(t), node_height(t), avl_ok ? "yes" : "no", bf1, nonleaf);
        node_free(t);
    }
}
