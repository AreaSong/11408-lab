#include "parse.h"

#include <errno.h>
#include <stdlib.h>

static void skip(const char **p) {
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

static int read_key(const char **p, int *key, int *rc) {
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(*p, &end, 10);
    if (end == *p || errno == ERANGE || value < RB_KEY_LO || value > RB_KEY_HI) {
        *rc = -1;
        return 0;
    }
    *p = end;
    *key = (int)value;
    return 1;
}

static RbNode *parse_node(RbTree *t, const char **p, int *rc, int *count);

static void drop(RbTree *t, RbNode *a, RbNode *b, RbNode *c) {
    rb_free_sub(t, a);
    rb_free_sub(t, b);
    rb_free_sub(t, c);
}

static RbNode *admit(RbTree *t, RbNode *node, int *rc, int *count) {
    (*count)++;
    if (*count > RB_KEY_MAX) {
        *rc = -2;
        rb_free_sub(t, node);
        return t->nil;
    }
    return node;
}

static RbNode *parse_kids(RbTree *t, RbNode *node, const char **p, int *rc, int *count) {
    RbNode *left;
    RbNode *right;
    (*p)++;
    left = parse_node(t, p, rc, count);
    skip(p);
    if (*rc || **p != ',') {
        if (!*rc) {
            *rc = -1;
        }
        drop(t, node, left, t->nil);
        return t->nil;
    }
    (*p)++;
    right = parse_node(t, p, rc, count);
    skip(p);
    if (*rc || **p != ')') {
        if (!*rc) {
            *rc = -1;
        }
        drop(t, node, left, right);
        return t->nil;
    }
    (*p)++;
    node->left = left;
    node->right = right;
    if (left != t->nil) {
        left->parent = node;
    }
    if (right != t->nil) {
        right->parent = node;
    }
    return node;
}

static RbNode *parse_node(RbTree *t, const char **p, int *rc, int *count) {
    RbNode *node;
    int key = 0;
    skip(p);
    if (**p == '#') {
        (*p)++;
        return t->nil;
    }
    if ((**p != '-' && (**p < '0' || **p > '9')) || !read_key(p, &key, rc)) {
        if (!*rc) {
            *rc = -1;
        }
        return t->nil;
    }
    skip(p);
    if ((*p)[0] != '[' || ((*p)[1] != 'B' && (*p)[1] != 'R') || (*p)[2] != ']') {
        *rc = -1;
        return t->nil;
    }
    node = rb_new(t, key, (*p)[1] == 'R');
    *p += 3;
    skip(p);
    if (**p == '(') {
        node = parse_kids(t, node, p, rc, count);
        if (*rc) {
            return t->nil;
        }
    }
    return admit(t, node, rc, count);
}

int rb_parse(RbTree *t, const char *s) {
    const char *p = s ? s : "";
    int rc = 0;
    int count = 0;
    RbNode *root = parse_node(t, &p, &rc, &count);
    if (rc) {
        rb_free_sub(t, root);
        t->root = t->nil;
        return rc;
    }
    skip(&p);
    if (*p != '\0') {
        rb_free_sub(t, root);
        t->root = t->nil;
        return -1;
    }
    t->root = root;
    if (root != t->nil) {
        root->parent = t->nil;
    }
    return 0;
}
