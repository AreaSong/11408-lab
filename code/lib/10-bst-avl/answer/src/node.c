#include "node.h"

#include <stdio.h>
#include <stdlib.h>

Node *node_new(int key) {
    Node *t = calloc(1, sizeof *t);
    if (!t) {
        exit(1);
    }
    t->key = key;
    t->height = 1;
    return t;
}

void node_free(Node *t) {
    if (!t) {
        return;
    }
    node_free(t->left);
    node_free(t->right);
    free(t);
}

Node *node_clone(const Node *t) {
    Node *n;
    if (!t) {
        return NULL;
    }
    n = node_new(t->key);
    n->height = t->height;
    n->left = node_clone(t->left);
    n->right = node_clone(t->right);
    return n;
}

int node_height(const Node *t) {
    return t ? t->height : 0;
}

int node_bf(const Node *t) {
    /* 王道：平衡因子 = 左子树高度 − 右子树高度。空树高度是 0，所以叶子是 0。 */
    return t ? node_height(t->left) - node_height(t->right) : 0;
}

void node_pull(Node *t) {
    int hl = node_height(t->left);
    int hr = node_height(t->right);
    t->height = (hl > hr ? hl : hr) + 1;
}

int node_count(const Node *t) {
    return t ? 1 + node_count(t->left) + node_count(t->right) : 0;
}

int node_equal(const Node *a, const Node *b) {
    if (!a || !b) {
        return a == b;
    }
    return a->key == b->key && node_equal(a->left, b->left) && node_equal(a->right, b->right);
}

static int order_ok(const Node *t, int *prev, int *has) {
    if (!t) {
        return 1;
    }
    if (!order_ok(t->left, prev, has)) {
        return 0;
    }
    /* 中序必须严格递增。相等关键字按王道不算合法二叉排序树。 */
    if (*has && t->key <= *prev) {
        return 0;
    }
    *has = 1;
    *prev = t->key;
    return order_ok(t->right, prev, has);
}

int node_bst_ok(const Node *t) {
    int prev = 0;
    int has = 0;
    return order_ok(t, &prev, &has);
}

static int height_rec(const Node *t) {
    int hl;
    int hr;
    int diff;
    int h;
    if (!t) {
        return 1;
    }
    if (!height_rec(t->left) || !height_rec(t->right)) {
        return 0;
    }
    hl = node_height(t->left);
    hr = node_height(t->right);
    diff = hl - hr;
    if (diff > 1 || diff < -1) {
        return 0;
    }
    h = (hl > hr ? hl : hr) + 1;
    return t->height == h;
}

int node_height_ok(const Node *t) {
    return height_rec(t);
}

const Node *node_find(const Node *t, int key) {
    while (t) {
        if (key == t->key) {
            return t;
        }
        t = key < t->key ? t->left : t->right;
    }
    return NULL;
}

void node_fprint(const Node *t, FILE *out, int with_bf) {
    if (!t) {
        fputc('#', out);
        return;
    }
    fprintf(out, "%d", t->key);
    if (with_bf) {
        fprintf(out, "[%d]", node_bf(t));
    }
    if (!t->left && !t->right) {
        return;
    }
    fputc('(', out);
    node_fprint(t->left, out, with_bf);
    fputc(',', out);
    node_fprint(t->right, out, with_bf);
    fputc(')', out);
}

int node_shape(const Node *t, char *buf, int cap, int with_bf) {
    char *mem = NULL;
    size_t size = 0;
    FILE *fp = open_memstream(&mem, &size);
    int n;
    if (!fp) {
        return -1;
    }
    node_fprint(t, fp, with_bf);
    fclose(fp);
    n = snprintf(buf, (size_t)cap, "%s", mem ? mem : "");
    free(mem);
    return n;
}

static void inorder_rec(const Node *t, FILE *out, int *first) {
    if (!t) {
        return;
    }
    inorder_rec(t->left, out, first);
    if (*first) {
        *first = 0;
    } else {
        fputc(' ', out);
    }
    fprintf(out, "%d", t->key);
    inorder_rec(t->right, out, first);
}

void node_inorder(const Node *t, FILE *out) {
    int first = 1;
    inorder_rec(t, out, &first);
}

static void levels_rec(const Node *t, int level, FILE *out, int *first) {
    if (!t) {
        return;
    }
    levels_rec(t->left, level + 1, out, first);
    if (*first) {
        *first = 0;
    } else {
        fputc(' ', out);
    }
    /* 根在第 1 层。查找成功时，这个层号就是比较次数。 */
    fprintf(out, "%d=%d", t->key, level);
    levels_rec(t->right, level + 1, out, first);
}

void node_levels(const Node *t, FILE *out) {
    int first = 1;
    levels_rec(t, 1, out, &first);
}
