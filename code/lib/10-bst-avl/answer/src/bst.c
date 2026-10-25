#include "bst.h"

#include <stdlib.h>

static Node *insert_rec(Node *t, int key, int *dup) {
    if (!t) {
        return node_new(key);
    }
    if (key == t->key) {
        *dup = 1;
        return t;
    }
    if (key < t->key) {
        t->left = insert_rec(t->left, key, dup);
    } else {
        t->right = insert_rec(t->right, key, dup);
    }
    node_pull(t);
    return t;
}

Node *bst_insert(Node *root, int key, int *dup) {
    *dup = 0;
    return insert_rec(root, key, dup);
}

static const char *case_name(const Node *t) {
    if (!t->left && !t->right) {
        return "leaf";
    }
    if (!t->right) {
        return "left";
    }
    if (!t->left) {
        return "right";
    }
    return "two";
}

/* 摘掉左子树最右结点，把它的关键字交出来。这个结点没有右孩子，左孩子接回原处。 */
static Node *detach_pred(Node *t, int *key_out) {
    if (!t->right) {
        Node *left = t->left;
        *key_out = t->key;
        free(t);
        return left;
    }
    t->right = detach_pred(t->right, key_out);
    node_pull(t);
    return t;
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
            fprintf(out, "case %s\n", case_name(t));
        }
        free(t);
        return child;
    } else {
        /* 左右都在：前驱顶上之后，被删的其实是前驱。前驱最多还有一棵左子树。 */
        *found = 1;
        t->left = detach_pred(t->left, &pred);
        if (report && out) {
            fprintf(out, "case two\npred %d\n", pred);
        }
        t->key = pred;
    }
    if (*found) {
        node_pull(t);
    }
    return t;
}

Node *bst_delete(Node *root, int key, FILE *out, int *found) {
    *found = 0;
    return delete_rec(root, key, out, found, 1);
}

int bst_search(const Node *root, int key, FILE *out) {
    const Node *t = root;
    while (t) {
        if (out) {
            fprintf(out, "cmp %d\n", t->key);
        }
        if (key == t->key) {
            return 1;
        }
        t = key < t->key ? t->left : t->right;
    }
    return 0;
}

static void asl_rec(const Node *t, int level, Asl *asl) {
    if (!t) {
        return;
    }
    asl->succ_num += level;
    asl->succ_den += 1;
    /* 空孩子就是一种失败。比较次数等于父结点的层号，不再把空指针本身算一次比较。 */
    if (!t->left) {
        asl->fail_num += level;
    }
    if (!t->right) {
        asl->fail_num += level;
    }
    asl_rec(t->left, level + 1, asl);
    asl_rec(t->right, level + 1, asl);
}

void bst_asl(const Node *root, Asl *asl) {
    asl->succ_num = 0;
    asl->succ_den = 0;
    asl->fail_num = 0;
    asl->fail_den = 0;
    asl_rec(root, 1, asl);
    asl->fail_den = asl->succ_den + 1;
}

static long long weight_of(int key, const int *keys, const int *weight, int n) {
    for (int i = 0; i < n; i++) {
        if (keys[i] == key) {
            return weight[i];
        }
    }
    return 0;
}

static void weight_rec(const Node *t, int level, const int *keys, const int *weight, int n,
                       long long *num) {
    if (!t) {
        return;
    }
    *num += (long long)level * weight_of(t->key, keys, weight, n);
    weight_rec(t->left, level + 1, keys, weight, n, num);
    weight_rec(t->right, level + 1, keys, weight, n, num);
}

void bst_asl_weight(const Node *root, const int *keys, const int *weight, int n, long long *num,
                    long long *den) {
    *num = 0;
    *den = 0;
    for (int i = 0; i < n; i++) {
        *den += weight[i];
    }
    weight_rec(root, 1, keys, weight, n, num);
}
