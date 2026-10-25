#include "scan.h"

#include "avl.h"
#include "node.h"

#include <stdio.h>
#include <string.h>

#define SHOT_CAP 768

typedef struct {
    int keys[8];
    int n;
    int victim;
    char before[SHOT_CAP];
    char after[SHOT_CAP];
} Shot;

typedef struct {
    Shot shot[4];
    int got[4];
} Bag;

static int avl_ok(const Node *t) {
    return node_bst_ok(t) && node_height_ok(t);
}

static void finish(FILE *out, const char *option, char pick) {
    fprintf(out, "option %s\n", option);
    fprintf(out, "pick=%c\n", pick);
}

static Node *build_avl(const int *keys, int n) {
    Node *t = NULL;
    for (int i = 0; i < n; i++) {
        int dup = 0;
        t = avl_insert(t, keys[i], 0, NULL, &dup);
    }
    return t;
}

static int is_leaf(const Node *t, int key) {
    const Node *p = node_find(t, key);
    return p && !p->left && !p->right;
}

static int next_perm(int *a, int n) {
    int i = n - 2;
    int j;
    int tmp;
    while (i >= 0 && a[i] >= a[i + 1]) {
        i--;
    }
    if (i < 0) {
        return 0;
    }
    j = n - 1;
    while (a[j] <= a[i]) {
        j--;
    }
    tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
    for (int l = i + 1, r = n - 1; l < r; l++, r--) {
        tmp = a[l];
        a[l] = a[r];
        a[r] = tmp;
    }
    return 1;
}

static int classify(const int *keys, int n, int victim, Bag *bag) {
    Node *orig = build_avl(keys, n);
    Node *now;
    int found = 0;
    int dup = 0;
    int leaf;
    int same;
    int slot;
    if (!avl_ok(orig)) {
        node_free(orig);
        return -1;
    }
    leaf = is_leaf(orig, victim);
    now = node_clone(orig);
    now = avl_delete(now, victim, NULL, &found);
    now = avl_insert(now, victim, 0, NULL, &dup);
    if (!found || dup || !avl_ok(now)) {
        node_free(orig);
        node_free(now);
        return -1;
    }
    same = node_equal(orig, now);
    /* 0 叶子且相同，1 叶子且不同，2 非叶且不同，3 非叶且相同。 */
    slot = leaf ? (same ? 0 : 1) : (same ? 3 : 2);
    if (!bag->got[slot]) {
        Shot *s = &bag->shot[slot];
        int nb;
        int na;
        s->n = n;
        s->victim = victim;
        for (int i = 0; i < n; i++) {
            s->keys[i] = keys[i];
        }
        nb = node_shape(orig, s->before, SHOT_CAP, 1);
        na = node_shape(now, s->after, SHOT_CAP, 1);
        bag->got[slot] = 1;
        if (nb < 0 || na < 0 || nb >= SHOT_CAP || na >= SHOT_CAP) {
            node_free(orig);
            node_free(now);
            return -1;
        }
    }
    node_free(orig);
    node_free(now);
    return 0;
}

static int find_shots(Bag *bag) {
    memset(bag, 0, sizeof *bag);
    for (int n = 2; n <= 8; n++) {
        int keys[8];
        for (int i = 0; i < n; i++) {
            keys[i] = i + 1;
        }
        do {
            for (int i = 0; i < n; i++) {
                if (classify(keys, n, keys[i], bag) < 0) {
                    return -1;
                }
            }
            if (bag->got[0] && bag->got[1] && bag->got[2] && bag->got[3]) {
                return 0;
            }
        } while (next_perm(keys, n));
    }
    return -1;
}

static void print_shot(FILE *out, const char *tag, const Shot *s, int same) {
    fprintf(out, "%s insert", tag);
    for (int i = 0; i < s->n; i++) {
        fprintf(out, " %d", s->keys[i]);
    }
    fprintf(out, " del %d\n", s->victim);
    fprintf(out, "before %s\n", s->before);
    fprintf(out, "after %s\n", s->after);
    fprintf(out, "same=%s\n", same ? "yes" : "no");
}

int exam_2019(FILE *out) {
    static const char *tag[] = {"leaf_same", "leaf_diff", "inner_diff", "inner_same"};
    static const int same[] = {1, 0, 0, 1};
    Bag bag;
    int ok;
    fprintf(out, "exam 2019-4\n");
    ok = find_shots(&bag) == 0;
    if (ok) {
        for (int i = 0; i < 4; i++) {
            print_shot(out, tag[i], &bag.shot[i], same[i]);
        }
    } else {
        fprintf(out, "sample=none\n");
    }
    finish(out, "A=I B=II C=I,II D=I,III", ok ? 'A' : '?');
    return 0;
}
