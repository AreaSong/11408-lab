#include "tree.h"

#include <stdlib.h>

RbTree *rb_create(void) {
    RbTree *t = calloc(1, sizeof *t);
    RbNode *nil = calloc(1, sizeof *nil);
    if (!t || !nil) {
        exit(1);
    }
    nil->left = nil->right = nil->parent = nil;
    nil->red = 0;
    t->nil = nil;
    t->root = nil;
    return t;
}

void rb_free_sub(RbTree *t, RbNode *n) {
    if (!n || n == t->nil) {
        return;
    }
    rb_free_sub(t, n->left);
    rb_free_sub(t, n->right);
    free(n);
}

void rb_free(RbTree *t) {
    if (!t) {
        return;
    }
    rb_free_sub(t, t->root);
    free(t->nil);
    free(t);
}

RbNode *rb_new(RbTree *t, int key, int red) {
    RbNode *n = calloc(1, sizeof *n);
    if (!n) {
        exit(1);
    }
    n->key = key;
    n->red = red ? 1 : 0;
    n->left = n->right = n->parent = t->nil;
    return n;
}

RbNode *rb_find(const RbTree *t, int key) {
    RbNode *n = t->root;
    while (n != t->nil) {
        if (key == n->key) {
            return n;
        }
        n = key < n->key ? n->left : n->right;
    }
    return t->nil;
}

RbNode *rb_min(const RbTree *t, RbNode *n) {
    while (n->left != t->nil) {
        n = n->left;
    }
    return n;
}

void rb_rotate_left(RbTree *t, RbNode *x) {
    RbNode *y = x->right;
    /* 哨兵全树只有一个。空孩子不改它的 parent，否则所有 NIL 会指向同一个父亲。 */
    x->right = y->left;
    if (y->left != t->nil) {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == t->nil) {
        t->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

void rb_rotate_right(RbTree *t, RbNode *x) {
    RbNode *y = x->left;
    x->left = y->right;
    if (y->right != t->nil) {
        y->right->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == t->nil) {
        t->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->right = x;
    x->parent = y;
}

void rb_fprint(const RbTree *t, const RbNode *n, FILE *out) {
    if (n == t->nil) {
        fputc('#', out);
        return;
    }
    fprintf(out, "%d[%c]", n->key, n->red ? 'R' : 'B');
    if (n->left == t->nil && n->right == t->nil) {
        return;
    }
    fputc('(', out);
    rb_fprint(t, n->left, out);
    fputc(',', out);
    rb_fprint(t, n->right, out);
    fputc(')', out);
}

static void inorder_rec(const RbTree *t, const RbNode *n, FILE *out, int *first) {
    if (n == t->nil) {
        return;
    }
    inorder_rec(t, n->left, out, first);
    if (*first) {
        *first = 0;
    } else {
        fputc(' ', out);
    }
    fprintf(out, "%d", n->key);
    inorder_rec(t, n->right, out, first);
}

void rb_print_inorder(const RbTree *t, FILE *out) {
    int first = 1;
    inorder_rec(t, t->root, out, &first);
}
