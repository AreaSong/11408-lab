#include "tree.h"

#include <string.h>

void tree_init(BiTree *t) {
    t->n = 0;
    t->root = 0;
}

int tree_alloc(BiTree *t, int data) {
    if (t->n + 1 >= BT_MAX) {
        return 0;
    }
    int id = ++t->n;
    t->node[id].data = data;
    t->node[id].left = 0;
    t->node[id].right = 0;
    return id;
}

static int build_pre(BiTree *t, const char *s, int *pos) {
    if (s[*pos] == '\0') {
        return -1;
    }
    unsigned char ch = (unsigned char)s[(*pos)++];
    if (ch == '#') {
        return 0;
    }
    int id = tree_alloc(t, ch);
    if (id == 0) {
        return -3;
    }
    int left = build_pre(t, s, pos);
    if (left < 0) {
        return left;
    }
    int right = build_pre(t, s, pos);
    if (right < 0) {
        return right;
    }
    t->node[id].left = left;
    t->node[id].right = right;
    return id;
}

int tree_from_pre(BiTree *t, const char *s) {
    tree_init(t);
    int pos = 0;
    int root = build_pre(t, s, &pos);
    if (root < 0) {
        tree_init(t);
        return root;
    }
    if (s[pos] != '\0') {
        tree_init(t);
        return -2;
    }
    t->root = root;
    return 0;
}

int tree_height(const BiTree *t, int id) {
    if (id == 0) {
        return 0;
    }
    int left = tree_height(t, t->node[id].left);
    int right = tree_height(t, t->node[id].right);
    return (left > right ? left : right) + 1;
}

static int write_ch(char *buf, int cap, int n, int ch) {
    if (n + 1 >= cap) {
        return -1;
    }
    buf[n] = (char)ch;
    buf[n + 1] = '\0';
    return n + 1;
}

static int write_pre(const BiTree *t, int id, char *buf, int cap, int n) {
    if (id == 0) {
        return n;
    }
    n = write_ch(buf, cap, n, t->node[id].data);
    if (n < 0) {
        return -1;
    }
    n = write_pre(t, t->node[id].left, buf, cap, n);
    if (n < 0) {
        return -1;
    }
    return write_pre(t, t->node[id].right, buf, cap, n);
}

static int write_in(const BiTree *t, int id, char *buf, int cap, int n) {
    if (id == 0) {
        return n;
    }
    n = write_in(t, t->node[id].left, buf, cap, n);
    if (n < 0) {
        return -1;
    }
    n = write_ch(buf, cap, n, t->node[id].data);
    if (n < 0) {
        return -1;
    }
    return write_in(t, t->node[id].right, buf, cap, n);
}

static int write_post(const BiTree *t, int id, char *buf, int cap, int n) {
    if (id == 0) {
        return n;
    }
    n = write_post(t, t->node[id].left, buf, cap, n);
    if (n < 0) {
        return -1;
    }
    n = write_post(t, t->node[id].right, buf, cap, n);
    if (n < 0) {
        return -1;
    }
    return write_ch(buf, cap, n, t->node[id].data);
}

int tree_pre(const BiTree *t, int id, char *buf, int cap) {
    buf[0] = '\0';
    return write_pre(t, id, buf, cap, 0) < 0 ? -1 : 0;
}

int tree_in(const BiTree *t, int id, char *buf, int cap) {
    buf[0] = '\0';
    return write_in(t, id, buf, cap, 0) < 0 ? -1 : 0;
}

int tree_post(const BiTree *t, int id, char *buf, int cap) {
    buf[0] = '\0';
    return write_post(t, id, buf, cap, 0) < 0 ? -1 : 0;
}

static void measure(const BiTree *t, int id, TreeInfo *o) {
    if (id == 0) {
        return;
    }
    int left = t->node[id].left;
    int right = t->node[id].right;
    int deg = (left != 0) + (right != 0);
    o->n++;
    if (left == 0) {
        o->nulls++;
    } else {
        o->no_left = 0;
    }
    if (right == 0) {
        o->nulls++;
    } else {
        o->no_right = 0;
    }
    if (deg == 0) {
        o->n0++;
    } else if (deg == 1) {
        o->n1++;
        o->nonleaf_deg2 = 0;
    } else {
        o->n2++;
        o->nonleaf_deg1 = 0;
    }
    measure(t, left, o);
    measure(t, right, o);
}

int tree_info(const BiTree *t, TreeInfo *info, char *pre, char *inord, char *post, int cap) {
    memset(info, 0, sizeof *info);
    info->no_left = 1;
    info->no_right = 1;
    info->nonleaf_deg1 = 1;
    info->nonleaf_deg2 = 1;
    measure(t, t->root, info);
    info->height = tree_height(t, t->root);
    info->nonleaf = info->n1 + info->n2;
    info->all_deg1 = info->n > 0 && info->n1 == info->n;
    info->all_deg2 = info->n > 0 && info->n2 == info->n;
    if (tree_pre(t, t->root, pre, cap) < 0 || tree_in(t, t->root, inord, cap) < 0 ||
        tree_post(t, t->root, post, cap) < 0) {
        return -1;
    }
    info->pre_eq_in = strcmp(pre, inord) == 0;
    return 0;
}

static char ch_of(const BiTree *t, int id) {
    return (char)t->node[id].data;
}

static void trace_pre(const BiTree *t, int id, FILE *out) {
    if (id == 0) {
        return;
    }
    char ch = ch_of(t, id);
    fprintf(out, "rec_pre enter %c\n", ch);
    fprintf(out, "rec_pre visit %c\n", ch);
    trace_pre(t, t->node[id].left, out);
    trace_pre(t, t->node[id].right, out);
    fprintf(out, "rec_pre leave %c\n", ch);
}

static void trace_in(const BiTree *t, int id, FILE *out) {
    if (id == 0) {
        return;
    }
    char ch = ch_of(t, id);
    fprintf(out, "rec_in enter %c\n", ch);
    trace_in(t, t->node[id].left, out);
    fprintf(out, "rec_in visit %c\n", ch);
    trace_in(t, t->node[id].right, out);
    fprintf(out, "rec_in leave %c\n", ch);
}

static void trace_post(const BiTree *t, int id, FILE *out) {
    if (id == 0) {
        return;
    }
    char ch = ch_of(t, id);
    fprintf(out, "rec_post enter %c\n", ch);
    trace_post(t, t->node[id].left, out);
    trace_post(t, t->node[id].right, out);
    fprintf(out, "rec_post visit %c\n", ch);
    fprintf(out, "rec_post leave %c\n", ch);
}

void tree_trace(const BiTree *t, FILE *out) {
    char pre[BT_MAX];
    char inord[BT_MAX];
    char post[BT_MAX];
    tree_pre(t, t->root, pre, BT_MAX);
    tree_in(t, t->root, inord, BT_MAX);
    tree_post(t, t->root, post, BT_MAX);
    trace_pre(t, t->root, out);
    fprintf(out, "rec_pre %s\n", pre);
    trace_in(t, t->root, out);
    fprintf(out, "rec_in %s\n", inord);
    trace_post(t, t->root, out);
    fprintf(out, "rec_post %s\n", post);
}
