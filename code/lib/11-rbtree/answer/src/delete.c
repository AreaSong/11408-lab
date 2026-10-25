#include "delete.h"

#include <stdarg.h>
#include <stdlib.h>

static void say(FILE *out, const char *fmt, ...) {
    va_list ap;
    if (!out) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
}

static void add_spin(int *spins) {
    if (spins) {
        (*spins)++;
    }
}

static void transplant(RbTree *t, RbNode *u, RbNode *v) {
    if (u->parent == t->nil) {
        t->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

static void print_case(const RbTree *t, RbNode *z, FILE *out) {
    RbNode *gone = z;
    if (!out) {
        return;
    }
    if (z->left != t->nil && z->right != t->nil) {
        gone = rb_min(t, z->right);
        say(out, "case two\n");
        say(out, "succ %d\n", gone->key);
    } else if (z->left == t->nil && z->right == t->nil) {
        say(out, "case leaf\n");
    } else if (z->right == t->nil) {
        say(out, "case left\n");
    } else {
        say(out, "case right\n");
    }
    say(out, "remove %d color=%c\n", gone->key, gone->red ? 'R' : 'B');
}

static void fix_left(RbTree *t, RbNode **xp, FILE *out, int *spins) {
    RbNode *x = *xp;
    RbNode *w = x->parent->right;
    int parent_key = x->parent->key;
    if (w->red) {
        say(out, "fix case=1 side=L sib=%d\n", w->key);
        say(out, "recolor sib %d B parent %d R\n", w->key, parent_key);
        say(out, "rotate %d left\n", parent_key);
        w->red = 0;
        x->parent->red = 1;
        rb_rotate_left(t, x->parent);
        add_spin(spins);
        w = x->parent->right;
    }
    if (!w->left->red && !w->right->red) {
        say(out, "fix case=2 side=L sib=%d\n", w->key);
        say(out, "recolor sib %d R\n", w->key);
        w->red = 1;
        *xp = x->parent;
        return;
    }
    if (!w->right->red) {
        say(out, "fix case=3 side=L sib=%d\n", w->key);
        say(out, "recolor near %d B sib %d R\n", w->left->key, w->key);
        say(out, "rotate %d right\n", w->key);
        w->left->red = 0;
        w->red = 1;
        rb_rotate_right(t, w);
        add_spin(spins);
        w = x->parent->right;
    }
    say(out, "fix case=4 side=L sib=%d\n", w->key);
    say(out, "recolor sib %d %c parent %d B far %d B\n", w->key, x->parent->red ? 'R' : 'B',
        x->parent->key, w->right->key);
    say(out, "rotate %d left\n", x->parent->key);
    w->red = x->parent->red;
    x->parent->red = 0;
    w->right->red = 0;
    rb_rotate_left(t, x->parent);
    add_spin(spins);
    *xp = t->root;
}

static void fix_right(RbTree *t, RbNode **xp, FILE *out, int *spins) {
    RbNode *x = *xp;
    RbNode *w = x->parent->left;
    int parent_key = x->parent->key;
    if (w->red) {
        say(out, "fix case=1 side=R sib=%d\n", w->key);
        say(out, "recolor sib %d B parent %d R\n", w->key, parent_key);
        say(out, "rotate %d right\n", parent_key);
        w->red = 0;
        x->parent->red = 1;
        rb_rotate_right(t, x->parent);
        add_spin(spins);
        w = x->parent->left;
    }
    if (!w->left->red && !w->right->red) {
        say(out, "fix case=2 side=R sib=%d\n", w->key);
        say(out, "recolor sib %d R\n", w->key);
        w->red = 1;
        *xp = x->parent;
        return;
    }
    if (!w->left->red) {
        say(out, "fix case=3 side=R sib=%d\n", w->key);
        say(out, "recolor near %d B sib %d R\n", w->right->key, w->key);
        say(out, "rotate %d left\n", w->key);
        w->right->red = 0;
        w->red = 1;
        rb_rotate_left(t, w);
        add_spin(spins);
        w = x->parent->left;
    }
    say(out, "fix case=4 side=R sib=%d\n", w->key);
    say(out, "recolor sib %d %c parent %d B far %d B\n", w->key, x->parent->red ? 'R' : 'B',
        x->parent->key, w->left->key);
    say(out, "rotate %d right\n", x->parent->key);
    w->red = x->parent->red;
    x->parent->red = 0;
    w->left->red = 0;
    rb_rotate_right(t, x->parent);
    add_spin(spins);
    *xp = t->root;
}

static void delete_fix(RbTree *t, RbNode *x, FILE *out, int *spins) {
    while (x != t->root && !x->red) {
        if (x == x->parent->left) {
            fix_left(t, &x, out, spins);
        } else {
            fix_right(t, &x, out, spins);
        }
    }
    /* 额外的黑落在红结点上时，把它涂黑就吸收了，这条路径的黑高补回。 */
    if (x != t->nil && x->red) {
        say(out, "absorb %d B\n", x->key);
    }
    x->red = 0;
}

static void clear_nil(RbTree *t) {
    t->nil->parent = t->nil;
    t->nil->left = t->nil;
    t->nil->right = t->nil;
    t->nil->red = 0;
}

int rb_delete(RbTree *t, int key, FILE *out, int *spins) {
    RbNode *z = rb_find(t, key);
    RbNode *y;
    RbNode *x;
    int y_was_red;
    if (spins) {
        *spins = 0;
    }
    if (z == t->nil) {
        return 1;
    }
    print_case(t, z, out);
    y = z;
    y_was_red = y->red;
    if (z->left == t->nil) {
        x = z->right;
        transplant(t, z, z->right);
    } else if (z->right == t->nil) {
        x = z->left;
        transplant(t, z, z->left);
    } else {
        y = rb_min(t, z->right);
        y_was_red = y->red;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        } else {
            transplant(t, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        transplant(t, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->red = z->red;
    }
    if (!y_was_red) {
        delete_fix(t, x, out, spins);
    }
    clear_nil(t);
    free(z);
    return 0;
}
